#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"
#include <utility/env.hpp>

#include <boost/json.hpp>
#include <curl/curl.h>
#include <grpc++/grpc++.h>
#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cstddef>
#include <cstring>
#include <expected>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace ls_gitea_runner {

enum class error_code {
    curl_error,
    fetch_task_failed,
    payload_encode_failed,
    payload_decode_failed,
    service_failure,
    workflow_parse_failed,
};

namespace {
size_t write_header_fn(const char* buffer, size_t size, size_t count, std::string* output) noexcept {
    try {
        output->append(buffer, size * count);
        return size * count;
    } catch (const std::exception&) {
        return 0;
    }
}

size_t write_body_fn(const void* buffer, size_t size, size_t count, std::vector<std::byte>* output) noexcept {
    try {
        const auto old_size{output->size()};
        output->resize(output->size() + size * count);
        auto* output_offset{output->data() + old_size};
        std::memcpy(output_offset, buffer, size * count);
        return size * count;
    } catch (const std::exception&) {
        return 0;
    }
}
} // namespace

struct http_response {
    int status{};
    std::vector<std::byte> body;
};

class http_header_source {
public:
    virtual void set_headers(std::function<auto(const std::string& name, const std::string& value)->void> cb) = 0;
};

class http_client {
public:
    http_client(const std::string& base_url, std::shared_ptr<http_header_source> header_source = {})
            : m_base_url{base_url}, m_header_source{std::move(header_source)} {
        if (!m_base_url.ends_with('/')) {
            m_base_url += '/';
        }
        m_base_url += "api/actions";
    }

    std::expected<http_response, error_code> post(const std::string& path,
                                                  const std::vector<std::byte>& payload) const noexcept {
        const auto url{m_base_url + path};

        std::string response_headers;
        std::vector<std::byte> response_body;

        curl_slist* headers{};
        headers = curl_slist_append(headers, "Accept: application/proto");
        headers = curl_slist_append(headers, "Content-Type: application/proto");

        m_header_source->set_headers([&](const std::string& name, const std::string& value) {
            std::string header{name};
            header += ": ";
            header += value;
            headers = curl_slist_append(headers, header.c_str());
        });

        CURLcode curl_code{CURLE_OK};
        long response_code{};
        auto* curl{curl_easy_init()};
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
            curl_easy_setopt(curl, CURLOPT_HEADER, 0);
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_fn);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_fn);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
            curl_code = curl_easy_perform(curl);
            if (curl_code == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            }
            curl_easy_cleanup(curl);
        }

        curl_slist_free_all(headers);

        if (!curl || curl_code != CURLE_OK) {
            return std::unexpected{error_code::curl_error};
        }

        return http_response{.body = response_body};
    }

private:
    std::string m_base_url;
    std::shared_ptr<http_header_source> m_header_source;
};

template <typename T> std::expected<std::vector<std::byte>, error_code> encode_payload(const T& msg) noexcept {
    const auto byte_size{msg.ByteSizeLong()};
    std::vector<std::byte> data;
    data.resize(static_cast<std::size_t>(byte_size));
    if (!msg.SerializeToArray(data.data(), byte_size)) {
        return std::unexpected{error_code::payload_encode_failed};
    }
    return data;
}

template <typename T> std::expected<T, error_code> decode_payload(const std::vector<std::byte>& payload) noexcept {
    T msg;
    if (!msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        return std::unexpected{error_code::payload_decode_failed};
    }
    return msg;
}

template <typename Request, typename Response>
std::expected<Response, error_code> send_post_request(const http_client& client, const std::string& path,
                                                      const Request& req) noexcept {
    return encode_payload(req)
            .and_then([&](auto payload) { return client.post(path, payload); })
            .and_then([](auto res) { return decode_payload<Response>(res.body); });
}

namespace ping {

std::expected<::ping::v1::PingResponse, error_code> ping(const http_client& client,
                                                         ::ping::v1::PingRequest req) noexcept {
    return send_post_request<::ping::v1::PingRequest, ::ping::v1::PingResponse>(client, "/ping.v1.PingService/Ping",
                                                                                req);
}

} // namespace ping

namespace runner {

std::expected<::runner::v1::RegisterResponse, error_code> register_(const http_client& client,
                                                                    ::runner::v1::RegisterRequest req) noexcept {
    return send_post_request<::runner::v1::RegisterRequest, ::runner::v1::RegisterResponse>(
            client, "/runner.v1.RunnerService/Register", req);
}

std::expected<::runner::v1::DeclareResponse, error_code> declare(const http_client& client,
                                                                 ::runner::v1::DeclareRequest req) noexcept {
    return send_post_request<::runner::v1::DeclareRequest, ::runner::v1::DeclareResponse>(
            client, "/runner.v1.RunnerService/Declare", req);
}

std::expected<::runner::v1::FetchTaskResponse, error_code> fetch_task(const http_client& client,
                                                                      ::runner::v1::FetchTaskRequest req) noexcept {
    return send_post_request<::runner::v1::FetchTaskRequest, ::runner::v1::FetchTaskResponse>(
            client, "/runner.v1.RunnerService/FetchTask", req);
}

std::expected<::runner::v1::UpdateTaskResponse, error_code> update_task(const http_client& client,
                                                                        ::runner::v1::UpdateTaskRequest req) noexcept {
    return send_post_request<::runner::v1::UpdateTaskRequest, ::runner::v1::UpdateTaskResponse>(
            client, "/runner.v1.RunnerService/UpdateTask", req);
}

std::expected<::runner::v1::UpdateLogResponse, error_code> update_log(const http_client& client,
                                                                      ::runner::v1::UpdateLogRequest req) noexcept {
    return send_post_request<::runner::v1::UpdateLogRequest, ::runner::v1::UpdateLogResponse>(
            client, "/runner.v1.RunnerService/UpdateLog", req);
}

} // namespace runner

class header_source : public http_header_source {
public:
    void set_headers(std::function<auto(const std::string& name, const std::string& value)->void> cb) {
        if (!m_uuid.empty()) {
            cb("X-Runner-UUID", m_uuid);
        }

        if (!m_token.empty()) {
            cb("X-Runner-Token", m_token);
        }
    }

    void set_uuid(const std::string& uuid) { m_uuid = uuid; }
    void set_token(const std::string& token) { m_token = token; }

private:
    std::string m_uuid;
    std::string m_token;
};

std::vector<std::string> parse_runner_labels_env_var(const std::string_view line) {
    auto labels{std::vector<std::string>{}};
    std::string::size_type a{}, b{};
    for (; (b = line.find_first_of(',', b)) != std::string::npos; a = ++b) {
        labels.emplace_back(line.substr(a, b - a));
    }
    if (line.length() - a > 0U) {
        labels.emplace_back(line.substr(a));
    }
    return labels;
}

struct runner_options {
    std::optional<std::string> instance;
    std::optional<std::string> runner_name;
    std::optional<std::string> runner_registration_token;
    std::vector<std::string> runner_labels;
    std::optional<std::string> runner_uuid;
    std::optional<std::string> runner_token;
    std::optional<bool> ephemeral{};

    static runner_options from_env() {
        const auto ephemeral_string{utility::getenv("GITEA_RUNNER_EPHEMERAL")};
        runner_options o{
                .instance{utility::getenv("GITEA_INSTANCE_URL")},
                .runner_name{utility::getenv("GITEA_RUNNER_NAME")},
                .runner_registration_token{utility::getenv("GITEA_RUNNER_REGISTRATION_TOKEN")},
                .runner_uuid{utility::getenv("GITEA_RUNNER_UUID")},
                .runner_token{utility::getenv("GITEA_RUNNER_TOKEN")},
                .ephemeral{ephemeral_string ? std::make_optional(*ephemeral_string == std::string{"1"})
                                            : std::make_optional<bool>()},
        };
        if (const auto labels{utility::getenv("GITEA_RUNNER_LABELS")}) {
            o.runner_labels = parse_runner_labels_env_var(*labels);
        }
        return o;
    }
};

google::protobuf::Timestamp current_timestamp() {
    using namespace std::literals;
    const auto time{std::chrono::system_clock::now().time_since_epoch()};
    const auto s{std::chrono::duration_cast<std::chrono::seconds>(time)};
    const auto ns{std::chrono::duration_cast<std::chrono::nanoseconds>(time - s)};
    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(s.count());
    timestamp.set_nanos(ns.count());
    return timestamp;
}

namespace workflow {
struct wf_step {
    std::optional<std::string> name;
    std::optional<std::string> run;
    std::optional<std::string> shell;
    std::optional<std::string> uses;
};

struct wf_job {
    std::vector<wf_step> steps;
};

std::vector<wf_step> load_steps(const YAML::Node& yaml_steps) {
    std::vector<wf_step> steps;
    for (auto& yaml_step : yaml_steps) {
        steps.push_back([&] {
            wf_step s;
            if (auto name{yaml_step["name"]}) {
                s.name = std::move(name.as<std::string>());
            }
            if (auto run{yaml_step["run"]}) {
                s.run = std::move(run.as<std::string>());
            }
            if (auto shell{yaml_step["shell"]}) {
                s.shell = std::move(shell.as<std::string>());
            }
            if (auto uses{yaml_step["uses"]}) {
                s.uses = std::move(uses.as<std::string>());
            }
            if (!s.run && !s.uses) {
                throw std::runtime_error{"Missing run/uses in workflow step"};
            }
            return s;
        }());
    }
    return steps;
}

std::expected<wf_job, error_code> load_job_with_name(const std::string& yaml_str, const std::string& name) noexcept {
    try {
        const auto yaml{YAML::Load(yaml_str)};
        const auto& yaml_jobs{yaml["jobs"]};
        const auto& yaml_job{yaml_jobs[name]};
        const auto& yaml_steps{yaml_job["steps"]};
        wf_job job{.steps = load_steps(yaml_steps)};
        return job;
    } catch (const std::exception&) {
        return std::unexpected{error_code::workflow_parse_failed};
    }
}

} // namespace workflow

std::expected<void, error_code>
process_task_response(const http_client& client, const ::runner::v1::FetchTaskResponse& fetch_task_response) noexcept {
    using namespace std::literals;
    const auto& task{fetch_task_response.task()};
    const auto& secrets{task.secrets()};
    const auto& needs{task.needs()};
    const auto& vars{task.vars()};
    const auto& context{task.context()};
    const auto& workflow_payload{task.workflow_payload()};
    const auto& job_name{context.fields().at("job").string_value()};

    const auto wf_job{workflow::load_job_with_name(workflow_payload, job_name)};
    if (!wf_job) {
        return std::unexpected{wf_job.error()};
    }

    // Initial task state
    auto update_task_request{::runner::v1::UpdateTaskRequest{}};
    auto* task_state{update_task_request.mutable_state()};
    {
        task_state->set_id(task.id());
        task_state->set_result(::runner::v1::RESULT_UNSPECIFIED);

        auto update_task_response{runner::update_task(client, update_task_request)};
        if (!update_task_response) {
            return std::unexpected{error_code::service_failure};
        }
    }

    // Set initial step state
    {
        const auto ts{current_timestamp()};
        task_state->mutable_started_at()->set_seconds(ts.seconds());
        task_state->mutable_started_at()->set_nanos(ts.nanos());

        int i{};
        for (const auto& wf_step : wf_job->steps) {
            auto* step_state{task_state->add_steps()};
            step_state->set_id(i);
            step_state->set_result(::runner::v1::RESULT_UNSPECIFIED);
            step_state->set_log_index(0);
            step_state->set_log_length(0);
            ++i;
        }

        auto update_task_response{runner::update_task(client, update_task_request)};
        if (!update_task_response) {
            return std::unexpected{error_code::service_failure};
        }
    }

    // Simulate work and completion of steps
    {
        for (auto& step_state : *task_state->mutable_steps()) {
            // Started
            {
                const auto ts{current_timestamp()};
                step_state.mutable_started_at()->set_seconds(ts.seconds());
                step_state.mutable_started_at()->set_nanos(ts.nanos());

                auto update_task_response{runner::update_task(client, update_task_request)};
                if (!update_task_response) {
                    return std::unexpected{error_code::service_failure};
                }
            }
            // "Work"
            std::this_thread::sleep_for(250ms);
            // Completed
            {
                const auto ts{current_timestamp()};
                step_state.mutable_stopped_at()->set_seconds(ts.seconds());
                step_state.mutable_stopped_at()->set_nanos(ts.nanos());
                step_state.set_result(::runner::v1::RESULT_SUCCESS);

                auto update_task_response{runner::update_task(client, update_task_request)};
                if (!update_task_response) {
                    return std::unexpected{error_code::service_failure};
                }
            }
        }
    }

    // Completion
    {
        const auto ts{current_timestamp()};
        task_state->mutable_stopped_at()->set_seconds(ts.seconds());
        task_state->mutable_stopped_at()->set_nanos(ts.nanos());
        task_state->set_result(::runner::v1::RESULT_SUCCESS);

        auto update_task_response{runner::update_task(client, update_task_request)};
        if (!update_task_response) {
            return std::unexpected{error_code::service_failure};
        }
    }

    return {};
}

std::expected<::runner::v1::FetchTaskResponse, error_code> wait_for_new_task(const http_client& client) noexcept {
    using namespace std::literals;
    auto fetch_task_response{std::expected<::runner::v1::FetchTaskResponse, error_code>{}};
    while (true) {
        auto fetch_task_request{::runner::v1::FetchTaskRequest{}};
        fetch_task_response = runner::fetch_task(client, fetch_task_request);
        if (!fetch_task_response) {
            return std::unexpected{error_code::fetch_task_failed};
        }

        if (!fetch_task_response->has_task()) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        break;
    }
    return fetch_task_response;
}

void run_task_loop(const http_client& client) noexcept {
    using namespace std::literals;
    while (true) {
        if (!wait_for_new_task(client).and_then(
                    [&](auto fetch_task_response) { return process_task_response(client, fetch_task_response); })) {
            std::println(std::cerr, "Error");
            std::this_thread::sleep_for(5s);
        }
    }
}

constexpr auto runner_version{std::string_view{"v0.1.0"}};

int cmd_register() noexcept {
    // GITEA_RUNNER_REGISTRATION_TOKEN_FILE

    const auto options{runner_options::from_env()};

    if (!options.instance) {
        std::println(std::cerr, "Missing instance URL");
        return 1;
    }

    if (!options.runner_name) {
        std::println(std::cerr, "Missing runner name");
        return 1;
    }

    if (!options.runner_registration_token) {
        std::println(std::cerr, "Missing runner registration token");
        return 1;
    }

    if (options.runner_labels.empty()) {
        std::println(std::cerr, "Missing runner labels");
        return 1;
    }

    auto header_source{std::make_shared<class header_source>()};
    http_client client{*options.instance, header_source};

    auto ping_request{::ping::v1::PingRequest{}};
    ping_request.set_data(*options.runner_name);
    auto ping_response{ping::ping(client, ping_request)};
    if (!ping_response) {
        std::println(std::cerr, "Error");
        return 1;
    }

    auto reqister_request{::runner::v1::RegisterRequest{}};
    reqister_request.set_name(*options.runner_name);
    reqister_request.set_token(*options.runner_registration_token);
    reqister_request.set_version(std::string{runner_version});
    for (auto& label : options.runner_labels) {
        reqister_request.add_labels(std::move(label));
    }
    if (options.ephemeral) {
        reqister_request.set_ephemeral(*options.ephemeral);
    }
    auto register_response{runner::register_(client, reqister_request)};
    if (!register_response) {
        std::println(std::cerr, "Error");
        return 1;
    }

    std::println("uuid={}", register_response->runner().uuid());
    std::println("token={}", register_response->runner().token());

    return 0;
}

int cmd_daemon() noexcept {
    using namespace std::literals;

    const auto options{runner_options::from_env()};

    if (!options.instance) {
        std::println(std::cerr, "Missing instance URL");
        return 1;
    }

    if (!options.runner_name) {
        std::println(std::cerr, "Missing runner name");
        return 1;
    }

    if (options.runner_labels.empty()) {
        std::println(std::cerr, "Missing runner labels");
        return 1;
    }

    if (!options.runner_uuid) {
        std::println(std::cerr, "Missing runner UUID");
        return 1;
    }

    if (!options.runner_token) {
        std::println(std::cerr, "Missing runner token");
        return 1;
    }

    auto header_source{std::make_shared<class header_source>()};
    http_client client{*options.instance, header_source};

    header_source->set_uuid(*options.runner_uuid);
    header_source->set_token(*options.runner_token);

    auto declare_request{::runner::v1::DeclareRequest{}};
    declare_request.set_version(std::string{runner_version});
    for (auto& label : options.runner_labels) {
        declare_request.add_labels(std::move(label));
    }
    auto declare_response{runner::declare(client, declare_request)};
    if (!declare_response) {
        std::println(std::cerr, "Error");
        return 1;
    }

    run_task_loop(client);

    return 0;
}

int main(int argc, char* const argv[]) {
    using namespace std::literals;

    if (argc < 2) {
        std::println(std::cerr, "Missing command");
        return 1;
    }

    const auto cmd{std::string_view{argv[1]}};
    if (cmd == "register"sv) {
        return cmd_register();
    } else if (cmd == "daemon"sv) {
        return cmd_daemon();
    } else {
        std::println(std::cerr, "Invalid command");
        return 1;
    }

    return 0;
}

} // namespace ls_gitea_runner

int main(int argc, char* const argv[]) { return ls_gitea_runner::main(argc, argv); }
