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

struct http_response {
    int status{};
    std::vector<std::byte> body;
};

class http_header_source {
public:
    virtual void set_headers(std::function<auto(const std::string& name, const std::string& value)->void> cb) = 0;
};

class http_client {
public:
    http_client(const std::string& base_url, std::shared_ptr<http_header_source> header_source = {});

    std::expected<http_response, generic_error> post(const std::string& path,
                                                     const std::vector<std::byte>& payload) const noexcept;

private:
    std::string m_base_url;
    std::shared_ptr<http_header_source> m_header_source;
};

} // namespace ls_gitea_runner
