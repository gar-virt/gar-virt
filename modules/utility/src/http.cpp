#include <utility/http.hpp>

#include <utility/algorithm.hpp>

#include <curl/curl.h>

#include <cstring>
#include <format>
#include <mutex>

namespace ls_gitea_runner::utility {

namespace {
size_t write_header_fn(const char* buffer, size_t size, size_t count, std::string* output) noexcept {
    try {
        output->append(buffer, size * count);
        return size * count;
    } catch (const std::exception&) {
        return 0;
    }
}

size_t write_body_fn(const void* buffer, size_t size, size_t count, std::vector<std::byte>* output) noexcept {
    try {
        const auto old_size{output->size()};
        output->resize(output->size() + (size * count));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto* output_offset{output->data() + old_size};
        std::memcpy(output_offset, buffer, size * count);
        return size * count;
    } catch (const std::exception&) {
        return 0;
    }
}

struct HttpStatusRange {
    enum Type { client_error_first = 400 };
};
} // namespace

std::string http_path_join(const std::string& first, const std::string& second) {
    const auto first_delim{first.ends_with('/')};
    const auto second_delim{second.starts_with('/')};
    if (first_delim && second_delim) {
        return first + second.substr(1);
    }
    if (!first_delim && !second_delim) {
        return first + "/" + second;
    }
    return first + second;
}

struct HttpClient::Private final {
    Private() : share{curl_share_init()} {
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_PSL);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_HSTS);
        curl_share_setopt(share, CURLSHOPT_USERDATA, this);

        constexpr auto lock_fn{+[](CURL* /*handle*/, curl_lock_data data, curl_lock_access /*access*/, void* clientp) {
            auto* self{static_cast<Private*>(clientp)};
            auto& curl_mutex{self->m_curl_mutexes.at(data)};
            curl_mutex.lock();
        }};
        curl_share_setopt(share, CURLSHOPT_LOCKFUNC, lock_fn);

        constexpr auto unlock_fn{+[](CURL* /*handle*/, curl_lock_data data, void* clientp) {
            auto* self{static_cast<Private*>(clientp)};
            auto& curl_mutex{self->m_curl_mutexes.at(data)};
            curl_mutex.unlock();
        }};
        curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, unlock_fn);
    }

    ~Private() {
        if (share) {
            curl_share_cleanup(share);
        }
    }

    Private(const Private&) = delete;
    Private(Private&&) = delete;

    Private& operator=(const Private&) = delete;
    Private& operator=(Private&&) = delete;

    CURLSH* share{};
    std::array<std::mutex, static_cast<size_t>(CURL_LOCK_DATA_LAST)> m_curl_mutexes;
};

HttpClient::HttpClient(const std::string& base_url) : m_priv{std::make_unique<Private>()}, m_base_url{base_url} {}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&& other) noexcept
        : m_priv{std::move(other.m_priv)}, m_base_url{std::move(other.m_base_url)},
          m_req_middlewares{std::move(other.m_req_middlewares)} {}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this != &other) {
        m_priv = std::move(other.m_priv);
        m_base_url = std::move(other.m_base_url);
        m_req_middlewares = std::move(other.m_req_middlewares);
    }
    return *this;
}

std::expected<HttpResponse, GenericError> HttpClient::send(HttpRequest req) const {
    for (const auto& req_middleware : m_req_middlewares) {
        if (!req_middleware(req)) {
            break;
        }
    }

    const auto url{http_path_join(m_base_url, req.path)};

    std::string response_headers;
    std::vector<std::byte> response_body;

    curl_slist* headers{};
    for (auto& h : req.headers) {
        std::string header{h.first};
        header += ": ";
        header += h.second;
        headers = curl_slist_append(headers, header.c_str());
    }

    CURLcode curl_code{CURLE_OK};
    long status_code{};
    auto* curl{curl_easy_init()};
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_SHARE, m_priv->share);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
        curl_easy_setopt(curl, CURLOPT_HEADER, 0);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_fn);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_fn);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        if (req.method == HttpMethod::post) {
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.payload.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.payload.size());
        } else if (req.method == HttpMethod::del) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }

        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
        }
        curl_easy_cleanup(curl);
    }

    curl_slist_free_all(headers);

    if (!curl || curl_code != CURLE_OK) {
        return std::unexpected{GenericError{
            std::format("HTTP request to \"{}\" failed (cURL error code: {})", url, static_cast<long>(curl_code))}};
    }

    const auto is_ok_already_deleted{status_code == 404 && req.method == HttpMethod::del};
    if (status_code >= HttpStatusRange::client_error_first && !is_ok_already_deleted) {
        return std::unexpected{
            GenericError{std::format("HTTP request to \"{}\" failed with status code {}", url, status_code)}};
    }

    return HttpResponse{.status = utility::safe_cast_int<int>(status_code), .body = response_body};
}

std::expected<HttpResponse, GenericError> HttpClient::post(std::string path, std::vector<std::byte> payload) const {
    return send(HttpRequest{
        .method = HttpMethod::post,
        .path = std::move(path),
        .payload = std::move(payload),
    });
}

std::expected<HttpResponse, GenericError> HttpClient::del(std::string path) const {
    return send(HttpRequest{
        .method = HttpMethod::del,
        .path = std::move(path),
    });
}

void HttpClient::add_request_middleware(HttpRequestMiddleware middleware) {
    m_req_middlewares.push_back(std::move(middleware));
}

} // namespace ls_gitea_runner::utility
