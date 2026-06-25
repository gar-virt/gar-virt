#pragma once

#include "utility/error.hpp"
#include "utility/http.hpp"

#include <expected>

namespace ls_gitea_runner::gitea {

class AdminServiceClient final {
public:
    AdminServiceClient(const std::string& instance_url, const std::string& token);

    std::expected<std::string, GenericError> get_registration_token() const noexcept;

private:
    std::string m_token;
    utility::HttpClient m_client;
};

} // namespace ls_gitea_runner::gitea
