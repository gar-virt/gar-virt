#pragma once

#include <utility/error.hpp>
#include <utility/http.hpp>

#include <cstdint>
#include <expected>

namespace gv::gitea {

class AdminServiceClient final {
public:
    AdminServiceClient(const std::string& instance_url, std::string token);
    ~AdminServiceClient();

    AdminServiceClient(const AdminServiceClient&) = delete;
    AdminServiceClient& operator=(const AdminServiceClient&) = delete;

    std::expected<std::string, Error> get_registration_token() const;
    std::expected<void, Error> remove_runner(uint64_t runner_id) const;

private:
    std::string m_token;
    utility::HttpClient m_client;
};

} // namespace gv::gitea
