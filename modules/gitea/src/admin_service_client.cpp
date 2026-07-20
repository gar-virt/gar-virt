#include <gitea/admin_service_client.hpp>

#include <utility/algorithm.hpp>

#include <boost/json.hpp>

#include <format>

namespace ls_gitea_runner::gitea {

AdminServiceClient::AdminServiceClient(const std::string& instance_url, const std::string& token)
        : m_token{token}, m_client{utility::http_path_join(instance_url, "/api/v1/admin")} {
    m_client.add_request_middleware([this](auto& req) {
        if (req.method == utility::HttpMethod::post) {
            req.headers.emplace("Content-Type", "application/json");
        }

        req.headers.emplace("Accept", "application/json");

        if (!m_token.empty()) {
            req.headers.emplace("Authorization", std::format("token {}", m_token));
        }

        return true;
    });
}

AdminServiceClient::~AdminServiceClient() = default;

std::expected<std::string, GenericError> AdminServiceClient::get_registration_token() const {
    auto res{m_client.post("/actions/runners/registration-token", {})};
    if (!res) {
        return std::unexpected{res.error()};
    }
    try {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* body{reinterpret_cast<const char*>(res->body.data())};
        const auto j{boost::json::parse(std::string_view{body, res->body.size()}).as_object()};
        return std::string{j.at("token").as_string()};
    } catch (const std::exception& ex) {
        return std::unexpected{GenericError{std::format("Failed to parse registration token: {}", ex.what())}};
    }
}

std::expected<void, GenericError> AdminServiceClient::remove_runner(uint64_t runner_id) const {
    return m_client.del(std::format("/actions/runners/{}", runner_id)).transform([](auto) {});
}

} // namespace ls_gitea_runner::gitea
