#pragma once

#include <utility/http.hpp>
#include <utility/result.hpp>

#include <cstdint>
#include <expected>

namespace gv::gitea {

class AdminServiceClient final {
public:
    AdminServiceClient(const std::string& instance_url, std::string token);
    ~AdminServiceClient();

    AdminServiceClient(const AdminServiceClient&) = delete;
    AdminServiceClient& operator=(const AdminServiceClient&) = delete;

    Result<std::string> get_registration_token() const;
    Result<void> remove_runner(uint64_t runner_id) const;

private:
    std::string m_token;
    utility::HttpClient m_client;
};

} // namespace gv::gitea
