#include "gitea_log_reporter.hpp"
#include "../protobuf_helper.hpp"

#include <utility/string.hpp>

namespace ls_gitea_runner::gitea {

gitea_log_reporter::gitea_log_reporter(const gitea_runner_service_client& client, const ::runner::v1::Task& task)
        : m_client{client}, m_task_id{task.id()} {}

void gitea_log_reporter::add(std::string message) {
    utility::string_split(message, '\n', [&](auto token) {
        std::scoped_lock lock{m_mutex};
        m_entries.emplace_back(protobuf::current_timestamp(), std::string{utility::string_trim(token)});
        ++m_head;
    });
}

void gitea_log_reporter::flush() {
    std::scoped_lock lock{m_mutex};
    flush_internal();
}

void gitea_log_reporter::close() {
    std::scoped_lock lock{m_mutex};
    m_done = true;
    flush_internal();
}

std::int64_t gitea_log_reporter::head() const {
    std::scoped_lock lock{m_mutex};
    return m_head;
}

bool gitea_log_reporter::flush_internal() {
    if (m_entries.empty()) {
        return true;
    }
    ::runner::v1::UpdateLogRequest req;
    req.set_task_id(m_task_id);
    req.set_index(m_bulk_index);
    req.set_no_more(m_done);
    for (auto& entry : m_entries) {
        auto* row{req.add_rows()};
        *row->mutable_time() = entry.timestamp;
        row->set_content(std::move(entry.content));
    }
    if (auto res{m_client.get().update_log(req)}) {
        m_bulk_index = m_head;
        // res->ack_index();
        m_entries.clear();
        return true;
    }
    return false;
}

} // namespace ls_gitea_runner::gitea
