#pragma once

#include "error.hpp"

#include <curl/curl.h>

#include <cstddef>
#include <cstring>
#include <expected>
#include <functional>
#include <memory>
#include <print>
#include <string>
#include <vector>

namespace ls_gitea_runner {

struct HttpResponse {
    int status{};
    std::vector<std::byte> body;
};

class HttpHeaderSource {
public:
    virtual void set_headers(std::function<auto(const std::string& name, const std::string& value)->void> cb) = 0;
};

class HttpClient {
public:
    HttpClient(const std::string& base_url, std::shared_ptr<HttpHeaderSource> header_source = {});

    std::expected<HttpResponse, GenericError> post(const std::string& path,
                                                   const std::vector<std::byte>& payload) const noexcept;

private:
    std::string m_base_url;
    std::shared_ptr<HttpHeaderSource> m_header_source;
};

} // namespace ls_gitea_runner
