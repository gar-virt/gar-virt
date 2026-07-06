#pragma once

#include <gitea/admin_service_client.hpp>
#include <gitea/runner_service_client.hpp>
#include <runner/v1/messages.pb.h>

#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace ls_gitea_runner::gitea {

struct RunnerOptions {
    std::string instance_url;
    std::string name;
    std::vector<std::string> labels;
    std::string version;

    std::vector<std::string> get_label_names() const {
        std::vector<std::string> items;
        for (auto label : labels) {
            auto pos{label.find_first_of(':')};
            if (pos != std::string::npos) {
                label = label.substr(0, pos);
            }
            items.push_back(std::move(label));
        }
        return items;
    }
};

class Runner final {
public:
    Runner(int64_t id, std::vector<std::string> labels, gitea::GiteaRunnerCredentials credentials,
           std::shared_ptr<gitea::GiteaRunnerServiceClient> client, std::shared_ptr<gitea::AdminServiceClient> admin);

    ~Runner();

    Runner(const Runner&) = delete;
    Runner(Runner&&) noexcept;

    Runner& operator=(const Runner&) = delete;
    Runner& operator=(Runner&&) noexcept;

    static std::expected<Runner, GenericError> connect(const RunnerOptions& options,
                                                       std::shared_ptr<gitea::AdminServiceClient> admin) noexcept;

    std::expected<::runner::v1::FetchTaskResponse, GenericError> fetch_task() noexcept;
    int64_t id() const noexcept;
    const gitea::GiteaRunnerCredentials& credentials() const noexcept;
    const gitea::GiteaRunnerServiceClient& client() const noexcept;
    const std::vector<std::string>& labels() const noexcept;

private:
    bool m_moved{};
    int64_t m_id{};
    std::vector<std::string> m_labels;
    gitea::GiteaRunnerCredentials m_credentials;
    std::shared_ptr<gitea::GiteaRunnerServiceClient> m_client;
    std::shared_ptr<gitea::AdminServiceClient> m_admin;
};

} // namespace ls_gitea_runner::gitea
