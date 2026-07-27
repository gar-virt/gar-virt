#pragma once

#include <gitea/fwd.hpp>
#include <gitea/runner_credentials.hpp>

#include <utility/result.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gv::gitea {

struct TaskParcel {
    std::int64_t id{};
    std::vector<std::byte> data;
};

struct RunnerOptions {
    std::string forge_uri;
    std::string name;
    std::vector<std::string> labels;
    std::string version;

    std::vector<std::string> get_label_names() const;
};

class Runner final {
public:
    Runner(int64_t id, std::vector<std::string> labels, std::string forge_uri,
           gitea::GiteaRunnerCredentials credentials, std::shared_ptr<gitea::GiteaRunnerServiceClient> client,
           std::shared_ptr<gitea::AdminServiceClient> admin);

    ~Runner();

    Runner(const Runner&) = delete;
    Runner(Runner&&) noexcept;

    Runner& operator=(const Runner&) = delete;
    Runner& operator=(Runner&&) noexcept;

    static Result<Runner> connect(RunnerOptions options, std::shared_ptr<gitea::AdminServiceClient> admin);

    Result<std::optional<TaskParcel>> fetch_task() const;
    int64_t id() const noexcept;
    const gitea::GiteaRunnerCredentials& credentials() const noexcept;
    const gitea::GiteaRunnerServiceClient& client() const noexcept;
    const std::vector<std::string>& labels() const noexcept;
    const std::string& forge_uri() const noexcept;
    void set_task_failed(const TaskParcel& task_parcel);

private:
    bool m_moved{};
    int64_t m_id{};
    std::vector<std::string> m_labels;
    std::string m_forge_uri;
    gitea::GiteaRunnerCredentials m_credentials;
    std::shared_ptr<gitea::GiteaRunnerServiceClient> m_client;
    std::shared_ptr<gitea::AdminServiceClient> m_admin;
};

} // namespace gv::gitea
