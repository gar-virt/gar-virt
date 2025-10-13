#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"
#include <utility/env.hpp>
#include <utility/string.hpp>
#include <utility/temporary_file.hpp>

#include <boost/json.hpp>
#include <curl/curl.h>
#include <grpc++/grpc++.h>
#include <mujs.h>
#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cstddef>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
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
struct wf_step_run {
    std::optional<std::string> shell;
    std::string script;
    std::optional<std::filesystem::path> working_directory;
};

struct wf_step_uses {
    std::string url;
};

struct wf_step {
    std::optional<std::string> name;
    std::optional<wf_step_run> run;
    std::optional<wf_step_uses> uses;
    std::optional<std::string> condition;
};

struct wf_job {
    std::vector<wf_step> steps;
    std::optional<std::string> condition;
};

std::vector<wf_step> load_steps(const YAML::Node& yaml_steps) {
    std::vector<wf_step> steps;
    for (auto& yaml_step : yaml_steps) {
        steps.push_back([&] {
            wf_step s;
            if (auto& condition{yaml_step["if"]}) {
                s.condition = std::move(condition.as<std::string>());
            }
            if (auto& name{yaml_step["name"]}) {
                s.name = std::move(name.as<std::string>());
            }
            auto& yaml_workdir{yaml_step["working-directory"]};
            auto workdir{yaml_workdir ? std::make_optional(std::filesystem::u8path(yaml_workdir.as<std::string>()))
                                      : std::nullopt};
            if (auto& run{yaml_step["run"]}) {
                auto& yaml_shell{yaml_step["shell"]};
                s.run = std::make_optional(wf_step_run{
                    .shell = yaml_shell ? std::make_optional(yaml_shell.as<std::string>()) : std::nullopt,
                    .script = run.as<std::string>(),
                    .working_directory = workdir,
                });
            }
            // Does "uses" also use working directory?
            if (auto& uses{yaml_step["uses"]}) {
                s.uses = std::make_optional(wf_step_uses{.url = std::move(uses.as<std::string>())});
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

struct step_execution_dependencies {
    using context_value = ::google::protobuf::Value;
    std::unordered_map<std::string, std::string> secrets;
    std::unordered_map<std::string, std::string> needs;
    std::unordered_map<std::string, std::string> vars;
    std::unordered_map<std::string, context_value> context;
};

struct step_execution_context {
    const workflow::wf_step* step{};
    ::runner::v1::StepState* state{};
    step_execution_dependencies dependencies;

    bool is_ok() noexcept {
        using namespace ::runner::v1;
        return state && (state->result() == RESULT_SKIPPED || state->result() == RESULT_SUCCESS);
    }
};

// This function was generated by Grok
template <typename Callable>
std::string regex_replace_callable(const std::string& text, const std::regex& pattern, Callable replacer) {
    std::string result;
    auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto end = std::sregex_iterator();
    size_t last_pos = 0;
    for (auto it = begin; it != end; ++it) {
        auto match = *it;
        result += text.substr(last_pos, match.position() - last_pos);
        result += replacer(match);
        last_pos = match.position() + match.length();
    }
    result += text.substr(last_pos);
    return result;
}

#if 0
std::string get_command_for_script_execution(const std::string& shell_name, const std::string& script_path) {
    if (shell_name == "bash") {

    } else if (shell_name == "pwsh") {

    } else if (shell_name == "powershell") {

    } else if (shell_name == "cmd") {
    }
    throw std::runtime_error{"Invalid shell name"};
}
#endif

enum class operating_system_id { windows, linux, macos };

std::string apply_string_substritutions(std::string script_str, const step_execution_dependencies& deps) {
    static const std::regex pattern{R"re(\$\{\{(.*?)}})re"};
    script_str = regex_replace_callable(script_str, pattern, [&](const std::smatch& m) -> std::string { return ""; });
    return script_str;
}

std::string get_default_shell(operating_system_id target_os) noexcept {
    using osid = operating_system_id;
    if (target_os == osid::windows) {
        return "pwsh";
    }
    return "bash";
}

bool is_execution_result_ok(::runner::v1::Result result) noexcept {
    using namespace ::runner::v1;
    return result == RESULT_SKIPPED || result == RESULT_SUCCESS;
}

bool execute_shell_script(const wf_step_run& input) {
    auto shell_name{input.shell.value_or("")};

    const auto script_str{input.script};

    fs::temporary_file real_script_file;
    {
        auto script_file{real_script_file.create_output_stream()};
        script_file->write(script_str.data(), script_str.size());
    }
    const auto real_script_path{utility::string_from_u8string(real_script_file.get_path().u8string())};
    const auto intermediate_script{std::format(R"script(
set -e
shell=${{1}}
script_file=${{2}}
script_file=${{script_file//\\//}}
if [[ -z "${{shell}}" ]]; then
    if [[ "${{OSTYPE}}" =~ cygwin|msys ]]; then
        shell=pwsh
    else
        shell=bash
    fi
fi
case "${{shell}}" in
    pwsh)
        pwsh -command ". \"${{script_file}}\""
        ;;
    powershell)
        powershell -command ". \"${{script_file}}\""
        ;;
    cmd)
        "${{ComSpec}}" /D /E:ON /V:OFF /S /C "CALL "${{script_file}}""
        ;;
    sh)
        sh -e "${{script_file}}"
        ;;
    bash)
        bash -e "${{script_file}}"
        ;;
    python)
        python -e "${{script_file}}"
        ;;
    *)
        echo "Invalid shell: ${{shell}}"
        exit 1
        ;;
esac
)script",
                                               shell_name, real_script_path)};
    fs::temporary_file intermeriate_script_file;
    {
        auto script_file{intermeriate_script_file.create_output_stream()};
        script_file->write(intermediate_script.data(), intermediate_script.size());
    }
    const auto intermeriate_script_path{utility::string_from_u8string(intermeriate_script_file.get_path().u8string())};
    const auto cmd{std::format("bash -e '{}' '{}' '{}'", intermeriate_script_path, shell_name, real_script_path)};
    // TODO: Check UTF-8 compatibility on Windows
    return std::system(cmd.c_str()) == 0;
}

bool execute_action(const wf_step_uses& input) {
    std::println("execute_action:\nurl: {}\n", input.url);
    return true;
}

::runner::v1::Result task_result_from_step_states(const std::vector<step_execution_context>& steps) noexcept {
    using namespace ::runner::v1;
    for (const auto& step : steps) {
        switch (step.state->result()) {
        case RESULT_SKIPPED:
        case RESULT_SUCCESS:
            continue;
        case RESULT_CANCELLED:
            return RESULT_CANCELLED;
        default:
            return RESULT_FAILURE;
        }
    }
    return RESULT_SUCCESS;
}

::runner::v1::Result execute_job_step(step_execution_context& ctx) {
    auto* step{ctx.step};
    auto* step_state{ctx.state};
    bool ok{};
    if (step->run) {
        ok = execute_shell_script(*step->run);
    } else if (step->uses) {
        ok = execute_action(*step->uses);
    }
    return ok ? ::runner::v1::RESULT_SUCCESS : ::runner::v1::RESULT_FAILURE;
}

} // namespace workflow

std::expected<void, error_code>
process_task_response(const http_client& client, const ::runner::v1::FetchTaskResponse& fetch_task_response) noexcept {
    using namespace std::literals;
    const auto& task{fetch_task_response.task()};
    const auto& task_context{task.context()};
    const auto& workflow_payload{task.workflow_payload()};
    const auto& job_name{task_context.fields().at("job").string_value()};

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

    std::vector<workflow::step_execution_context> step_executions;

    workflow::step_execution_dependencies dependencies;
    for (auto& item : task.secrets()) {
        dependencies.secrets.emplace(std::move(item.first), std::move(item.second));
    }
    // TODO: needs
    for (auto& item : task.vars()) {
        dependencies.vars.emplace(std::move(item.first), std::move(item.second));
    }
    for (auto& item : task_context.fields()) {
        dependencies.context.emplace(std::move(item.first), std::move(item.second));
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
            step_executions.push_back({.step = &wf_step, .state = step_state, .dependencies{dependencies}});
            ++i;
        }

        auto update_task_response{runner::update_task(client, update_task_request)};
        if (!update_task_response) {
            return std::unexpected{error_code::service_failure};
        }
    }

    // Simulate work and completion of steps
    {
        for (auto& step_execution : step_executions) {
            auto* step{step_execution.step};
            auto* step_state{step_execution.state};
            // Started
            {
                const auto ts{current_timestamp()};
                step_state->mutable_started_at()->set_seconds(ts.seconds());
                step_state->mutable_started_at()->set_nanos(ts.nanos());

                auto update_task_response{runner::update_task(client, update_task_request)};
                if (!update_task_response) {
                    return std::unexpected{error_code::service_failure};
                }
            }
            auto step_result{::runner::v1::RESULT_UNSPECIFIED};
            // "Work"
            bool should_exec{true};
            if (step->condition) {
                js_State* jss{js_newstate(nullptr, nullptr, JS_STRICT)};
                js_newcfunction(
                    jss,
                    +[](js_State* jss_) {
                        const std::string_view a{js_tostring(jss_, 1)};
                        const std::string_view b{js_tostring(jss_, 2)};
                        js_pushboolean(jss_, a.starts_with(b));
                    },
                    "startsWith", 2);
                    js_setglobal(jss, "startsWith");
                if (js_dostring(jss, step->condition->c_str()) != 0) {
                    step_result = ::runner::v1::RESULT_FAILURE;
                }
                js_freestate(jss);
            }
            if (step_result == ::runner::v1::RESULT_UNSPECIFIED) {
                if (should_exec) {
                    step_result = execute_job_step(step_execution);
                } else {
                    step_result = ::runner::v1::RESULT_SKIPPED;
                }
            }
            // Completed
            {
                const auto ts{current_timestamp()};
                step_state->mutable_stopped_at()->set_seconds(ts.seconds());
                step_state->mutable_stopped_at()->set_nanos(ts.nanos());
                step_state->set_result(step_result);

                auto update_task_response{runner::update_task(client, update_task_request)};
                if (!update_task_response) {
                    return std::unexpected{error_code::service_failure};
                }
            }
            if (!workflow::is_execution_result_ok(step_result)) {
                break;
            }
        }
    }

    // Completion
    {
        const auto task_result{task_result_from_step_states(step_executions)};
        const auto ts{current_timestamp()};
        task_state->mutable_stopped_at()->set_seconds(ts.seconds());
        task_state->mutable_stopped_at()->set_nanos(ts.nanos());
        task_state->set_result(task_result);

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
