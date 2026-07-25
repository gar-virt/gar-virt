export module gitea:admin;

import utility.http;
import utility.misc;

import std;
import std.compat;

namespace ls_gitea_runner::gitea {

export class AdminServiceClient final {
public:
    AdminServiceClient(const std::string& instance_url, std::string token);
    ~AdminServiceClient();
    std::expected<std::string, GenericError> get_registration_token() const;
    std::expected<void, GenericError> remove_runner(uint64_t runner_id) const;

private:
    std::string m_token;
    utility::HttpClient m_client;
};

} // namespace ls_gitea_runner::gitea
