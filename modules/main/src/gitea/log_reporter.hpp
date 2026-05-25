#pragma once

#include "../log_reporter.hpp"
#include "runner_service_client.hpp"
#include "runner/v1/messages.pb.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace ls_gitea_runner::gitea {

class GiteaLogReporter final : public LogReporter {
    struct Entry {
        google::protobuf::Timestamp timestamp;
        std::string content;
    };

public:
    GiteaLogReporter(const GiteaRunnerServiceClient& client, const ::runner::v1::Task& task);
    void add(std::string message) override;
    void flush() override;
    void close();
    std::int64_t head() const;

private:
    bool flush_internal();

    std::reference_wrapper<const GiteaRunnerServiceClient> m_client;
    std::int64_t m_task_id{};
    std::int64_t m_bulk_index{};
    std::int64_t m_head{};
    std::vector<Entry> m_entries;
    bool m_done{};
    mutable std::mutex m_mutex;
};

} // namespace ls_gitea_runner::gitea
