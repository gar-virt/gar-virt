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

HttpClient::HttpClient(const std::string& base_url, std::shared_ptr<HttpHeaderSource> header_source)
        : m_base_url{base_url}, m_header_source{std::move(header_source)} {
    if (!m_base_url.ends_with('/')) {
        m_base_url += '/';
    }
    m_base_url += "api/actions";
}

std::expected<HttpResponse, GenericError> HttpClient::post(const std::string& path,
                                                           const std::vector<std::byte>& payload) const noexcept {
    const auto url{m_base_url + path};

    std::string response_headers;
    std::vector<std::byte> response_body;

    curl_slist* headers{};
    headers = curl_slist_append(headers, "Accept: application/proto");
    headers = curl_slist_append(headers, "Content-Type: application/proto");

    m_header_source->set_headers([&](const std::string& name, const std::string& value) {
        std::string header{name};
        header += ": ";
        header += value;
        headers = curl_slist_append(headers, header.c_str());
    });

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
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
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

} // namespace ls_gitea_runner
