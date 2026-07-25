module;

#include <runner/v1/messages.pb.h>

export module gitea:runner;

import :admin;
import :credentials;
import :runner_service;

import utility.misc;

import std;

namespace ls_gitea_runner::gitea {

export struct RunnerOptions {
    std::string forge_uri;
    std::string name;
    std::vector<std::string> labels;
    std::string version;

    std::vector<std::string> get_label_names() const;
};

export class Runner final {
public:
    Runner(int64_t id, std::vector<std::string> labels, std::string forge_uri,
           gitea::GiteaRunnerCredentials credentials, std::shared_ptr<gitea::GiteaRunnerServiceClient> client,
           std::shared_ptr<gitea::AdminServiceClient> admin);

    ~Runner();

    Runner(const Runner&) noexcept;
    Runner(Runner&& other) noexcept;

    Runner& operator=(const Runner&) noexcept;
    Runner& operator=(Runner&& other) noexcept;

    static std::expected<Runner, GenericError> connect(RunnerOptions options,
                                                       std::shared_ptr<gitea::AdminServiceClient> admin);

    std::expected<::runner::v1::FetchTaskResponse, GenericError> fetch_task() const;

    int64_t id() const noexcept;
    const gitea::GiteaRunnerCredentials& credentials() const noexcept;
    const gitea::GiteaRunnerServiceClient& client() const noexcept;
    const std::vector<std::string>& labels() const noexcept;
    const std::string& forge_uri() const noexcept;

    void set_task_failed(const ::runner::v1::Task& task);

private:
    bool m_moved{};
    int64_t m_id{};
    std::vector<std::string> m_labels;
    std::string m_forge_uri;
    gitea::GiteaRunnerCredentials m_credentials;
    std::shared_ptr<gitea::GiteaRunnerServiceClient> m_client;
    std::shared_ptr<gitea::AdminServiceClient> m_admin;
};

} // namespace ls_gitea_runner::gitea
