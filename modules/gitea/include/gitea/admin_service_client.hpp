#pragma once

#include "utility/error.hpp"
#include "utility/http.hpp"

#include <cstdint>
#include <expected>

namespace ls_gitea_runner::gitea {

class AdminServiceClient final {
public:
    AdminServiceClient(const std::string& instance_url, const std::string& token);
    ~AdminServiceClient();

    AdminServiceClient(const AdminServiceClient&) = delete;
    AdminServiceClient& operator=(const AdminServiceClient&) = delete;

    std::expected<std::string, GenericError> get_registration_token() const noexcept;
    std::expected<void, GenericError> remove_runner(uint64_t runner_id) const noexcept;

private:
    std::string m_token;
    utility::HttpClient m_client;
};

} // namespace ls_gitea_runner::gitea
