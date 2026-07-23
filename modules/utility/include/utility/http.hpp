#pragma once

#include "error.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ls_gitea_runner::utility {

std::string http_path_join(const std::string& first, const std::string& second);

enum class HttpMethod { get, post, del };

struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::vector<std::byte> payload;
    std::map<std::string, std::string> headers;
};

struct HttpResponse {
    int status{};
    std::vector<std::byte> body;
};

using HttpRequestMiddleware = std::function<bool(HttpRequest& req)>;

class HttpClient {
public:
    HttpClient(std::string base_url);
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;

    HttpClient& operator=(const HttpClient& other) = delete;
    HttpClient& operator=(HttpClient&& other) noexcept;

    std::expected<HttpResponse, GenericError> send(HttpRequest req) const;

    std::expected<HttpResponse, GenericError> post(std::string path, std::vector<std::byte> payload) const;
    std::expected<HttpResponse, GenericError> del(std::string path) const;

    void add_request_middleware(HttpRequestMiddleware middleware);

private:
    struct Private;
    std::unique_ptr<Private> m_priv;
    std::string m_base_url;
    std::vector<HttpRequestMiddleware> m_req_middlewares;
};

} // namespace ls_gitea_runner::utility
