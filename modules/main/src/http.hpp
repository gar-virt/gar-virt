#pragma once

#include "error.hpp"

#include <cstddef>
#include <cstring>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <print>
#include <string>
#include <vector>

namespace ls_gitea_runner {

enum class HttpMethod { get, post };

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
    HttpClient(const std::string& base_url);
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;

    HttpClient& operator=(const HttpClient& other) = delete;
    HttpClient& operator=(HttpClient&& other) noexcept;

    std::expected<HttpResponse, GenericError> send(HttpRequest req) const noexcept;

    std::expected<HttpResponse, GenericError> post(std::string path, std::vector<std::byte> payload) const noexcept;

    void add_request_middleware(HttpRequestMiddleware middleware);

private:
    struct Private;
    std::unique_ptr<Private> m_priv;
    std::string m_base_url;
    std::vector<HttpRequestMiddleware> m_req_middlewares;
};

} // namespace ls_gitea_runner
