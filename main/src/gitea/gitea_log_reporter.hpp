#pragma once

#include "../log_reporter.hpp"
#include "gitea_runner_service_client.hpp"
#include "runner/v1/messages.pb.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ls_gitea_runner::gitea {

class gitea_log_reporter final : public log_reporter {
    struct entry_t {
        google::protobuf::Timestamp timestamp;
        std::string content;
    };

public:
    gitea_log_reporter(const gitea_runner_service_client& client, const ::runner::v1::Task& task);
    void add(std::string message) override;
    void flush() override;
    void close();
    std::int64_t head() const;

private:
    bool flush_internal();

    std::reference_wrapper<const gitea_runner_service_client> m_client;
    std::int64_t m_task_id{};
    std::int64_t m_bulk_index{};
    std::int64_t m_head{};
    std::vector<entry_t> m_entries;
    bool m_done{};
};

} // namespace ls_gitea_runner::gitea
