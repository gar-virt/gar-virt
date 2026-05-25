#include "http.hpp"

#include <curl/curl.h>

#include <cstring>
#include <format>

namespace ls_gitea_runner {

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
        output->resize(output->size() + size * count);
        auto* output_offset{output->data() + old_size};
        std::memcpy(output_offset, buffer, size * count);
        return size * count;
    } catch (const std::exception&) {
        return 0;
    }
}
} // namespace

HttpClient::HttpClient(const std::string& base_url) : m_base_url{base_url} {
    if (!m_base_url.ends_with('/')) {
        m_base_url += '/';
    }
    m_base_url += "api/actions";
}

std::expected<HttpResponse, GenericError> HttpClient::send(HttpRequest req) const noexcept {
    for (auto& req_middleware : m_req_middlewares) {
        if (!req_middleware(req)) {
            break;
        }
    }

    const auto url{m_base_url + req.path};

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
    long response_code{};
    auto* curl{curl_easy_init()};
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_HEADER, 0);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_fn);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_fn);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.payload.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.payload.size());
        curl_code = curl_easy_perform(curl);
        if (curl_code == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        }
        curl_easy_cleanup(curl);
    }

    curl_slist_free_all(headers);

    if (!curl || curl_code != CURLE_OK) {
        return std::unexpected{
            GenericError{std::format("HTTP request to \"{}\" failed (cURL error code: {}; HTTP status code: {})", url,
                                     static_cast<long>(curl_code), response_code)}};
    }

    return HttpResponse{.body = response_body};
}

std::expected<HttpResponse, GenericError> HttpClient::post(std::string path,
                                                           std::vector<std::byte> payload) const noexcept {
    return send(HttpRequest{
        .method = HttpMethod::post,
        .path = std::move(path),
        .payload = std::move(payload),
    });
}

void HttpClient::add_request_middleware(HttpRequestMiddleware middleware) {
    m_req_middlewares.push_back(std::move(middleware));
}

} // namespace ls_gitea_runner
