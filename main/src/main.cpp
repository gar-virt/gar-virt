#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"
#include <utility/env.hpp>
#include <utility/string.hpp>
#include <utility/temporary_file.hpp>

#include <boost/json.hpp>
#include <boost/program_options.hpp>
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
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <regex>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ls_gitea_runner {

class generic_error : public std::runtime_error {
public:
    generic_error(const std::string& message, std::source_location sloc = std::source_location::current())
            : runtime_error{message}, m_sloc{sloc} {}

    const std::source_location& where() const noexcept { return m_sloc; }

private:
    std::source_location m_sloc;
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

    std::expected<http_response, generic_error> post(const std::string& path,
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
            return std::unexpected{generic_error{
                std::format("HTTP request failed to \"{}\" failed (cURL error code: {}; HTTP status code: {})", url,
                            static_cast<long>(curl_code), response_code)}};
        }

        return http_response{.body = response_body};
    }

private:
    std::string m_base_url;
    std::shared_ptr<http_header_source> m_header_source;
};

template <typename T> std::expected<std::vector<std::byte>, generic_error> encode_payload(const T& msg) noexcept {
    const auto byte_size{msg.ByteSizeLong()};
    std::vector<std::byte> data;
    data.resize(static_cast<std::size_t>(byte_size));
    if (!msg.SerializeToArray(data.data(), byte_size)) {
        return std::unexpected{generic_error{"Failed to encode gRPC message"}};
    }
    return data;
}

template <typename T> std::expected<T, generic_error> decode_payload(const std::vector<std::byte>& payload) noexcept {
    T msg;
    if (!msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        return std::unexpected{generic_error{"Failed to decode gRPC message"}};
    }
    return msg;
}

template <typename Request, typename Response>
std::expected<Response, generic_error> send_post_request(const http_client& client, const std::string& path,
                                                         const Request& req) noexcept {
    return encode_payload(req)
        .and_then([&](auto payload) { return client.post(path, payload); })
        .and_then([](auto res) { return decode_payload<Response>(res.body); });
}

boost::json::value protobuf_to_json(const ::google::protobuf::Value& from);
boost::json::value protobuf_to_json(const ::google::protobuf::Struct& from);
boost::json::value protobuf_to_json(const ::google::protobuf::ListValue& from);

boost::json::value protobuf_to_json(const ::google::protobuf::Value& from) {
    if (from.has_bool_value()) {
        return from.bool_value();
    }
    if (from.has_string_value()) {
        return boost::json::string{from.string_value()};
    }
    if (from.has_number_value()) {
        return from.number_value();
    }
    if (from.has_null_value()) {
        return nullptr;
    }
    if (from.has_struct_value()) {
        return protobuf_to_json(from.struct_value());
    }
    if (from.has_list_value()) {
        return protobuf_to_json(from.list_value());
    }
    throw generic_error{"Can't map unknown protobuf value type to JSON"};
}

boost::json::value protobuf_to_json(const ::google::protobuf::Struct& from) {
    boost::json::object result;
    for (auto& entry : from.fields()) {
        result[entry.first] = protobuf_to_json(entry.second);
    }
    return result;
}

boost::json::value protobuf_to_json(const ::google::protobuf::ListValue& from) {
    boost::json::array result;
    for (auto& entry : from.values()) {
        result.push_back(protobuf_to_json(entry));
    }
    return result;
}

boost::json::value protobuf_to_json(const ::google::protobuf::Map<std::string, std::string>& from) {
    boost::json::object result;
    for (auto& entry : from) {
        result[entry.first] = entry.second;
    }
    return result;
}

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

namespace scripting {
namespace builtin_fn {

bool contains(const std::string_view search, const std::string_view item) noexcept { return search.contains(item); }

void register_contains(js_State* jss) {
    js_newcfunction(
        jss,
        +[](js_State* jss_) {
            const auto* arg1{js_tostring(jss_, 1)};
            const auto* arg2{js_tostring(jss_, 2)};
            const auto result{builtin_fn::contains(arg1, arg2)};
            js_pushboolean(jss_, result);
        },
        "contains", 2);
    js_setglobal(jss, "contains");
}

bool endsWith(const std::string_view searchString, const std::string_view searchValue) noexcept {
    return searchString.ends_with(searchValue);
}

void register_endsWith(js_State* jss) {
    js_newcfunction(
        jss,
        +[](js_State* jss_) {
            const auto* arg1{js_tostring(jss_, 1)};
            const auto* arg2{js_tostring(jss_, 2)};
            const auto result{builtin_fn::endsWith(arg1, arg2)};
            js_pushboolean(jss_, result);
        },
        "endsWith", 2);
    js_setglobal(jss, "endsWith");
}

bool startsWith(const std::string_view searchString, const std::string_view searchValue) noexcept {
    return searchString.starts_with(searchValue);
}

void register_startsWith(js_State* jss) {
    js_newcfunction(
        jss,
        +[](js_State* jss_) {
            const auto* arg1{js_tostring(jss_, 1)};
            const auto* arg2{js_tostring(jss_, 2)};
            const auto result{builtin_fn::startsWith(arg1, arg2)};
            js_pushboolean(jss_, result);
        },
        "startsWith", 2);
    js_setglobal(jss, "startsWith");
}

} // namespace builtin_fn

class expression_evaluator final {
public:
    using value_t = std::variant<std::string, bool, double, std::nullptr_t>;

    expression_evaluator(const std::vector<std::pair<std::string, std::string>>& global_objects)
            : m_jss{js_newstate(nullptr, nullptr, JS_STRICT)} {
        js_setreport(
            m_jss,
            +[](js_State* jss_, const char* message) { std::println("Expression evaluation error: {}", message); });
        builtin_fn::register_contains(m_jss);
        builtin_fn::register_endsWith(m_jss);
        builtin_fn::register_startsWith(m_jss);
        add_global_objects(global_objects);
    }

    ~expression_evaluator() { js_freestate(m_jss); }

    std::expected<value_t, generic_error> eval(const std::string& expr) {
        if (js_ploadstring(m_jss, "[script]", expr.c_str())) {
            return std::unexpected{generic_error{std::format("Failed to load expression <{}>", expr)}};
        }

        // Push the "this" value
        js_pushglobal(m_jss);

        if (js_pcall(m_jss, 0)) {
            // Pop the "this" value?
            js_pop(m_jss, 1);
            return std::unexpected{generic_error{std::format("Evaluation of expression <{}> failed", expr)}};
        }

        std::expected<value_t, generic_error> eval_result;

        if (js_isboolean(m_jss, -1)) {
            eval_result = js_toboolean(m_jss, -1) != 0;
        } else if (js_isstring(m_jss, -1)) {
            eval_result = std::string{js_tostring(m_jss, -1)};
        } else if (js_isnumber(m_jss, -1)) {
            eval_result = js_tonumber(m_jss, -1);
        } else if (js_isnull(m_jss, -1)) {
            eval_result = nullptr;
        } else {
            return std::unexpected{
                generic_error{std::format("Evaluation of expression <{}> resulted in unsupported type", expr)}};
        }

        // TODO: object, array, NaN?

        // Pop result
        js_pop(m_jss, 1);
        return eval_result;
    }

    std::expected<bool, generic_error> eval_true(const std::string& expr) {
        const auto expr_{std::format("!!({})", expr)};
        return eval(expr_).transform([](auto res) {
            auto bool_res{std::get<bool>(res)};
            return bool_res;
        });
    }

private:
    void add_global_objects(const std::vector<std::pair<std::string, std::string>>& global_objects) {
        using namespace std::literals;
        std::string script;
        for (auto& [k, v] : global_objects) {
            script += std::format("var {} = {}\n", k, v);
        }
        js_dostring(m_jss, script.c_str());
    }

    js_State* m_jss{};
};

struct apply_string_substitutions_visitor {
    apply_string_substitutions_visitor(std::string& result) : m_result{result} {}

    std::string operator()(std::nullptr_t) { return m_result = "null"; }
    std::string operator()(bool v) { return m_result = v ? "true" : "false"; }
    std::string operator()(double v) { return m_result = std::format("{}", v); }
    std::string operator()(const std::string& v) { return m_result = v; }

    // TODO: object, array

private:
    std::string& m_result;
};

std::string apply_string_substitutions(std::string script_str,
                                       const std::vector<std::pair<std::string, std::string>>& contexts) {
    static const std::regex pattern{R"re(\$\{\{\s*(.*?)\s*}})re"};
    script_str = regex_replace_callable(script_str, pattern, [&](const std::smatch& m) -> std::string {
        scripting::expression_evaluator expr_eval{contexts};
        const auto& expr{m[1].str()};
        if (auto eval_result{expr_eval.eval(expr)}) {
            std::string result_value;
            std::visit(apply_string_substitutions_visitor{result_value}, *eval_result);
            return result_value;
        }
        // TODO: report error
        return "null";
    });
    return script_str;
}

} // namespace scripting

namespace ping {

std::expected<::ping::v1::PingResponse, generic_error> ping(const http_client& client,
                                                            ::ping::v1::PingRequest req) noexcept {
    return send_post_request<::ping::v1::PingRequest, ::ping::v1::PingResponse>(client, "/ping.v1.PingService/Ping",
                                                                                req);
}

} // namespace ping

namespace runner {

std::expected<::runner::v1::RegisterResponse, generic_error> register_(const http_client& client,
                                                                       ::runner::v1::RegisterRequest req) noexcept {
    return send_post_request<::runner::v1::RegisterRequest, ::runner::v1::RegisterResponse>(
        client, "/runner.v1.RunnerService/Register", req);
}

std::expected<::runner::v1::DeclareResponse, generic_error> declare(const http_client& client,
                                                                    ::runner::v1::DeclareRequest req) noexcept {
    return send_post_request<::runner::v1::DeclareRequest, ::runner::v1::DeclareResponse>(
        client, "/runner.v1.RunnerService/Declare", req);
}

std::expected<::runner::v1::FetchTaskResponse, generic_error> fetch_task(const http_client& client,
                                                                         ::runner::v1::FetchTaskRequest req) noexcept {
    return send_post_request<::runner::v1::FetchTaskRequest, ::runner::v1::FetchTaskResponse>(
        client, "/runner.v1.RunnerService/FetchTask", req);
}

std::expected<::runner::v1::UpdateTaskResponse, generic_error>
update_task(const http_client& client, ::runner::v1::UpdateTaskRequest req) noexcept {
    return send_post_request<::runner::v1::UpdateTaskRequest, ::runner::v1::UpdateTaskResponse>(
        client, "/runner.v1.RunnerService/UpdateTask", req);
}

std::expected<::runner::v1::UpdateLogResponse, generic_error> update_log(const http_client& client,
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
    YAML::Node yaml;
};

struct wf_job {
    std::vector<wf_step> steps;
    std::optional<std::string> condition;
};

struct workflow_contexts {
    std::string main;    // main context as a JSON object
    std::string vars;    // configuration variables  as a JSON object
    std::string secrets; // secrets  as a JSON object
    std::string runner;  // info about current runner
    std::string matrix;  // current matrix combination

    std::vector<std::pair<std::string, std::string>> to_list() const {
        std::vector<std::pair<std::string, std::string>> result;
        result.emplace_back("gitea", main);
        result.emplace_back("github", "gitea");
        result.emplace_back("vars", vars);
        result.emplace_back("secrets", secrets);
        result.emplace_back("runner", runner);
        result.emplace_back("matrix", matrix);
        return result;
    }
};

std::vector<wf_step> load_steps(const YAML::Node& yaml_steps, const workflow_contexts& contexts) {
    const auto sub{scripting::apply_string_substitutions};
    const auto& contexts_list{contexts.to_list()};
    std::vector<wf_step> steps;
    for (auto& yaml_step : yaml_steps) {
        steps.push_back([&] {
            wf_step s;
            if (auto& condition{yaml_step["if"]}) {
                s.condition = sub(condition.as<std::string>(), contexts_list);
            }
            if (auto& name{yaml_step["name"]}) {
                s.name = sub(name.as<std::string>(), contexts_list);
            }
            auto& yaml_workdir{yaml_step["working-directory"]};
            auto workdir{yaml_workdir ? std::make_optional(
                                            std::filesystem::u8path(sub(yaml_workdir.as<std::string>(), contexts_list)))
                                      : std::nullopt};
            if (auto& run{yaml_step["run"]}) {
                auto& yaml_shell{yaml_step["shell"]};
                s.run = std::make_optional(wf_step_run{
                    .shell = yaml_shell ? std::make_optional(sub(yaml_shell.as<std::string>(), contexts_list))
                                        : std::nullopt,
                    .script = sub(run.as<std::string>(), contexts_list),
                    .working_directory = workdir,
                });
            }
            // Does "uses" also use working directory?
            if (auto& uses{yaml_step["uses"]}) {
                s.uses = std::make_optional(wf_step_uses{.url = sub(uses.as<std::string>(), contexts_list)});
            }
            if (!s.run && !s.uses) {
                throw generic_error{"Missing run/uses in workflow step"};
            }
            s.yaml = yaml_step.as<YAML::Node>();
            return s;
        }());
    }
    return steps;
}

std::expected<YAML::Node, generic_error> load_workflow_yaml(const std::string& yaml_str) noexcept {
    try {
        return YAML::Load(yaml_str);
    } catch (const std::exception&) {
        return std::unexpected{generic_error{"Failed to parse workflow YAML"}};
    }
}

std::expected<YAML::Node, generic_error> find_job_with_name_in_yaml(const YAML::Node& yaml,
                                                                    const std::string& name) noexcept {
    try {
        const auto& yaml_jobs{yaml["jobs"]};
        if (!yaml_jobs) {
            return std::unexpected{generic_error{"Missing jobs in workflow YAML"}};
        }
        const auto& yaml_job{yaml_jobs[name]};
        if (!yaml_job) {
            return std::unexpected{generic_error{std::format("Missing job named \"{}\" in workflow YAML", name)}};
        }
        return yaml_job;
    } catch (const std::exception& ex) {
        return std::unexpected{
            generic_error{std::format("Error while looking for job in workflow YAML: {}", ex.what())}};
    }
}

std::expected<wf_job, generic_error> load_job_with_name(const YAML::Node& yaml, const std::string& name,
                                                        const workflow_contexts& contexts) noexcept {
    try {
        const auto& yaml_job{find_job_with_name_in_yaml(yaml, name)};
        if (!yaml_job) {
            return std::unexpected{generic_error{std::format("Couldn't find job named \"{}\" in workflow YAML", name)}};
        }
        const auto& yaml_steps{(*yaml_job)["steps"]};
        wf_job job{.steps = load_steps(yaml_steps, contexts)};
        return job;
    } catch (const std::exception& ex) {
        return std::unexpected{
            generic_error{std::format("Error while loading job named \"{}\" in workflow YAML: {}", name, ex.what())}};
    }
}

using workflow_env = std::shared_ptr<std::unordered_map<std::string, std::string>>;

std::string load_matrix_context_from_job_yaml(const YAML::Node& yaml) {
    const auto& yaml_strategy{yaml["strategy"]};
    if (!yaml_strategy) {
        return "{}";
    }
    const auto& yaml_matrix{yaml_strategy["matrix"]};
    if (!yaml_matrix) {
        return "{}";
    }
    if (!yaml_matrix.IsMap()) {
        return "{}";
    }
    boost::json::object json;
    for (auto& entry : yaml_matrix) {
        if (!entry.second.IsSequence()) {
            continue;
        }
        // TODO: better error handling
        try {
            const auto& key{entry.first.as<std::string>()};
            const auto& values{entry.second.as<std::vector<std::string>>()};
            json[key] = values.at(0);
        } catch (const std::exception&) {
            // Ignore
        }
    }
    return boost::json::serialize(json);
}

std::string create_runner_context() {
    std::string script;
    script += "{";
    script += R"(name:"fake-name",)";
    script += R"(os:"fake-os",)";
    script += R"(arch:"fake-arch",)";
    script += R"(temp:"fake-temp",)";
    script += R"(tool_cache:"fake-tool-cache",)";
    script += R"(debug:"fake-debug",)";
    script += R"(environment:"fake-environment",)";
    script += "}";
    return script;
}

struct step_execution_context {
    const workflow::wf_step* step{};
    ::runner::v1::StepState* state{};
    workflow_contexts wf_contexts;

    bool is_ok() noexcept {
        using namespace ::runner::v1;
        return state && (state->result() == RESULT_SKIPPED || state->result() == RESULT_SUCCESS);
    }
};

bool is_execution_result_ok(::runner::v1::Result result) noexcept {
    using namespace ::runner::v1;
    return result == RESULT_SKIPPED || result == RESULT_SUCCESS;
}

std::string generate_shell_execution_intermediate_script(const std::string& shell_name,
                                                         const std::string& real_script_path) {
    static constexpr auto format_string{R"script(
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
)script"};
    const auto script{std::format(format_string, shell_name, real_script_path)};
    return script;
}

bool execute_shell_script(const wf_step_run& input, const workflow::workflow_env& env) {
    auto shell_name{input.shell.value_or("")};
    const auto script_str{input.script};
    fs::temporary_file real_script_file;
    {
        auto script_file{real_script_file.create_output_stream()};
        script_file->write(script_str.data(), script_str.size());
    }
    const auto real_script_path{utility::string_from_u8string(real_script_file.get_path().u8string())};
    const auto intermediate_script{generate_shell_execution_intermediate_script(shell_name, real_script_path)};
    fs::temporary_file intermeriate_script_file;
    {
        auto script_file{intermeriate_script_file.create_output_stream()};
        script_file->write(intermediate_script.data(), intermediate_script.size());
    }
    const auto intermeriate_script_path{utility::string_from_u8string(intermeriate_script_file.get_path().u8string())};
    const auto cmd{std::format("bash -e '{}' '{}' '{}'", intermeriate_script_path, shell_name, real_script_path)};

    // Back up environment variables
    std::unordered_map<std::string, std::optional<std::string>> original_env;
    for (const auto& [k, v] : *env) {
        original_env[k] = utility::getenv(k);
    }

    // Set environment variables
    for (const auto& [k, v] : *env) {
        utility::setenv(k, v);
    }

    // TODO: Check UTF-8 compatibility on Windows
    bool exec_ok{std::system(cmd.c_str()) == 0};

    // Restore environment variables
    for (const auto& [k, v] : original_env) {
        if (v) {
            utility::setenv(k, *v);
        } else {
            utility::unsetenv(k);
        }
    }

    return exec_ok;
}

bool execute_action(const wf_step_uses& input, const workflow::workflow_env& env) {
    std::println("TODO: execute_action(url: {})", input.url);
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

::runner::v1::Result execute_job_step(step_execution_context& ctx, const workflow::workflow_env& env) {
    auto* step{ctx.step};
    auto* step_state{ctx.state};
    bool ok{};
    if (step->run) {
        ok = execute_shell_script(*step->run, env);
    } else if (step->uses) {
        ok = execute_action(*step->uses, env);
    }
    return ok ? ::runner::v1::RESULT_SUCCESS : ::runner::v1::RESULT_FAILURE;
}

} // namespace workflow

namespace config {
struct docker_config {
    std::string image;
    std::string tag;
};

struct qemu_config {
    std::string image;
    std::string cpu;
    std::size_t memory;
};

struct runner_environment_config {
    std::vector<std::string> labels;
    std::string os;
    std::variant<docker_config, qemu_config> details;
};

struct runner_config {
    std::string instance_url;
    std::string name;
    std::string token;
    bool ephemeral{};
    std::unordered_map<std::string, runner_environment_config> environments;
};

std::expected<runner_config, generic_error> load_file(const std::filesystem::path& file_path) noexcept {
    try {
        std::ifstream is{file_path, std::ios_base::binary};
        const auto j{boost::json::parse(is).as_object()};
        runner_config c{
            .instance_url = std::string{j.at("instance_url").as_string()},
            .name = std::string{j.at("name").as_string()},
            .token = std::string{j.at("token").as_string()},
            .ephemeral = j.at("ephemeral").as_bool(),
        };
        for (auto& [env_key, env_value] : j.at("environments").as_object()) {
            const auto& env_labels{env_value.at("labels").as_array()};
            const auto& env_details{env_value.at("details").as_object()};
            c.environments[env_key] = {
                .labels =
                    [&] {
                        std::vector<std::string> labels;
                        labels.reserve(env_labels.size());
                        std::transform(env_labels.begin(), env_labels.end(), std::back_inserter(labels),
                                       [](auto& l) { return std::string{l.as_string()}; });
                        return labels;
                    }(),
                .os = std::string{env_value.at("os").as_string()},
                .details = [&] -> decltype(runner_environment_config::details) {
                    if (env_key == "docker") {
                        return docker_config{
                            .image = std::string{env_details.at("image").as_string()},
                            .tag = std::string{env_details.at("tag").as_string()},
                        };
                    } else if (env_key == "qemu") {
                        return qemu_config{
                            .image = std::string{env_details.at("image").as_string()},
                            .cpu = std::string{env_details.at("cpu").as_string()},
                            .memory = utility::safe_cast_int<decltype(qemu_config::memory)>(
                                env_details.at("memory").as_int64()),
                        };
                    }
                    throw generic_error{"Invalid runner environment type in config"};
                }(),
            };
        }
        return c;
    } catch (const std::exception& ex) {
        return std::unexpected{
            generic_error{std::format("Error while loading config file \"{}\": {}",
                                      utility::string_from_u8string(file_path.u8string()), ex.what())}};
    }
}
} // namespace config

struct program_options {
    std::filesystem::path config_file;
    std::filesystem::path state_file;
};

struct runtime_state {
    std::string uuid;
    std::string token;

    std::expected<void, generic_error> save() {
        try {
            std::ostringstream oss{std::ios_base::binary};
            boost::json::object o = {
                {"uuid", uuid},
                {"token", token},
            };
            oss << boost::json::serialize(o);
            std::ofstream ofs{m_file_path, std::ios_base::binary};
            if (!ofs.is_open()) {
                return std::unexpected{generic_error{std::format(
                    "Unable to open file for writing: {}", utility::string_from_u8string(m_file_path.u8string()))}};
            }
            ofs << oss.str();
            return {};
        } catch (const std::exception& ex) {
            return std::unexpected{
                generic_error{std::format("Failed to save state file \"{}\": {}",
                                          utility::string_from_u8string(m_file_path.u8string()), ex.what())}};
        }
    }

    static std::expected<runtime_state, generic_error> load_file(const std::filesystem::path& file_path) {
        try {
            std::ifstream is{file_path, std::ios_base::binary};
            if (!is.is_open()) {
                return std::unexpected{generic_error{std::format("Unable to open file for reading: {}",
                                                                 utility::string_from_u8string(file_path.u8string()))}};
            }
            const auto json{boost::json::parse(is).as_object()};
            runtime_state result{file_path};
            result.uuid = std::string{json.at("uuid").as_string()};
            result.token = std::string{json.at("token").as_string()};
            return result;
        } catch (const std::exception& ex) {
            return std::unexpected{
                generic_error{std::format("Failed to load state file \"{}\": {}",
                                          utility::string_from_u8string(file_path.u8string()), ex.what())}};
        }
    }

    static std::expected<runtime_state, generic_error> create(const std::filesystem::path& file_path) {
        runtime_state result{file_path};
        return result;
    }

private:
    runtime_state(const std::filesystem::path& file_path) : m_file_path{file_path} {}

    std::filesystem::path m_file_path;
};

workflow::workflow_env load_and_derive_env_from_yaml(const YAML::Node& yaml, workflow::workflow_env env,
                                                     workflow::workflow_contexts& contexts) {
    if (!env) {
        env = std::make_shared<workflow::workflow_env::element_type>();
    }
    if (!yaml) {
        return env;
    }
    const auto& yaml_env{yaml["env"]};
    if (!yaml_env || !yaml_env.IsMap()) {
        return env;
    }
    const auto sub{scripting::apply_string_substitutions};
    const auto& contexts_list{contexts.to_list()};
    auto env_copy{std::make_shared<workflow::workflow_env::element_type>(*env)};
    for (auto& entry : yaml_env) {
        if (!entry.second.IsScalar()) {
            continue;
        }
        // TODO: may need conversion to string or error handling
        const auto& key{entry.first.as<std::string>()};
        auto value{sub(entry.second.as<std::string>(), contexts_list)};
        (*env_copy)[key] = std::move(value);
    }
    return env_copy;
}

boost::json::value fixup_main_context_json(boost::json::value json) {
    json.as_object()["gitea"].as_object()["workspace"] =
        utility::string_from_u8string(std::filesystem::current_path().u8string());
    return json;
}

std::expected<void, generic_error> process_task_response(const http_client& client,
                                                         const ::runner::v1::FetchTaskResponse& fetch_task_response,
                                                         const config::runner_config& config) noexcept {
    using namespace std::literals;
    const auto& task{fetch_task_response.task()};
    const auto& task_context{task.context()};
    const auto& job_name{task_context.fields().at("job").string_value()};

    auto workflow_payload{workflow::load_workflow_yaml(task.workflow_payload())};
    if (!workflow_payload) {
        return std::unexpected{workflow_payload.error()};
    }

    const auto yaml_job{workflow::find_job_with_name_in_yaml(*workflow_payload, job_name)};
    if (!yaml_job) {
        return std::unexpected{yaml_job.error()};
    }

    workflow::workflow_contexts wf_contexts;
    try {
        wf_contexts.main = boost::json::serialize(fixup_main_context_json(protobuf_to_json(task.context())));
        wf_contexts.vars = boost::json::serialize(protobuf_to_json(task.vars()));
        wf_contexts.secrets = boost::json::serialize(protobuf_to_json(task.secrets()));
        wf_contexts.runner = workflow::create_runner_context();
        wf_contexts.matrix = workflow::load_matrix_context_from_job_yaml(*yaml_job);
    } catch (const std::exception&) {
        return std::unexpected{generic_error{std::format("Failed to build contexts for task #{}", task.id())}};
    }

    const auto global_env{load_and_derive_env_from_yaml(*workflow_payload, {}, wf_contexts)};
    const auto job_env{load_and_derive_env_from_yaml(*yaml_job, global_env, wf_contexts)};

    const auto wf_job{workflow::load_job_with_name(*workflow_payload, job_name, wf_contexts)};
    if (!wf_job) {
        return std::unexpected{wf_job.error()};
    }

    const auto create_update_task_failure_error{
        [&] { return std::unexpected{generic_error{std::format("Failed to update #{}", task.id())}}; }};

    // Initial task state
    auto update_task_request{::runner::v1::UpdateTaskRequest{}};
    auto* task_state{update_task_request.mutable_state()};
    {
        task_state->set_id(task.id());
        task_state->set_result(::runner::v1::RESULT_UNSPECIFIED);

        auto update_task_response{runner::update_task(client, update_task_request)};
        if (!update_task_response) {
            return create_update_task_failure_error();
        }
    }

    std::vector<workflow::step_execution_context> step_executions;

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
            step_executions.push_back({.step = &wf_step, .state = step_state, .wf_contexts = wf_contexts});
            ++i;
        }

        auto update_task_response{runner::update_task(client, update_task_request)};
        if (!update_task_response) {
            return create_update_task_failure_error();
        }
    }

    // Execute steps while updating status
    {
        for (auto& step_execution : step_executions) {
            auto* step{step_execution.step};
            auto* step_state{step_execution.state};
            const auto step_env{load_and_derive_env_from_yaml(step->yaml, job_env, wf_contexts)};
            scripting::expression_evaluator evaluator{step_execution.wf_contexts.to_list()};
            // Started
            {
                const auto ts{current_timestamp()};
                step_state->mutable_started_at()->set_seconds(ts.seconds());
                step_state->mutable_started_at()->set_nanos(ts.nanos());

                auto update_task_response{runner::update_task(client, update_task_request)};
                if (!update_task_response) {
                    return create_update_task_failure_error();
                }
            }
            auto step_result{::runner::v1::RESULT_UNSPECIFIED};
            // "Work"
            if (step->condition) {
                const auto eval_result{evaluator.eval_true(*step->condition)};
                if (!eval_result) {
                    step_result = ::runner::v1::RESULT_FAILURE;
                } else if (!*eval_result) {
                    step_result = ::runner::v1::RESULT_SKIPPED;
                }
            }
            if (step_result == ::runner::v1::RESULT_UNSPECIFIED) {
                step_result = execute_job_step(step_execution, step_env);
            }
            // Completed
            {
                const auto ts{current_timestamp()};
                step_state->mutable_stopped_at()->set_seconds(ts.seconds());
                step_state->mutable_stopped_at()->set_nanos(ts.nanos());
                step_state->set_result(step_result);

                auto update_task_response{runner::update_task(client, update_task_request)};
                if (!update_task_response) {
                    return create_update_task_failure_error();
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
            return create_update_task_failure_error();
        }
    }

    return {};
}

std::expected<::runner::v1::FetchTaskResponse, generic_error> wait_for_new_task(const http_client& client) noexcept {
    using namespace std::literals;
    auto fetch_task_response{std::expected<::runner::v1::FetchTaskResponse, generic_error>{}};
    while (true) {
        auto fetch_task_request{::runner::v1::FetchTaskRequest{}};
        fetch_task_response = runner::fetch_task(client, fetch_task_request);
        if (!fetch_task_response) {
            return std::unexpected{generic_error{"Failed to fetch any new tasks"}};
        }

        if (!fetch_task_response->has_task()) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        break;
    }
    return fetch_task_response;
}

void run_task_loop(const http_client& client, const config::runner_config& config) noexcept {
    using namespace std::literals;
    while (true) {
        if (auto res{wait_for_new_task(client).and_then([&](auto fetch_task_response) {
                return process_task_response(client, fetch_task_response, config);
            })}) {
            std::this_thread::sleep_for(250ms);
        } else {
            std::println(std::cerr, "Error: {}", res.error().what());
            std::this_thread::sleep_for(5s);
        }
    }
}

constexpr auto runner_version{std::string_view{"v0.1.0"}};

int cmd_register(config::runner_config config, runtime_state state) noexcept {
    // GITEA_RUNNER_REGISTRATION_TOKEN_FILE

    auto header_source{std::make_shared<class header_source>()};
    http_client client{config.instance_url, header_source};

    auto ping_request{::ping::v1::PingRequest{}};
    ping_request.set_data(config.name);
    auto ping_response{ping::ping(client, ping_request)};
    if (!ping_response) {
        std::println(std::cerr, "Error");
        return 1;
    }

    auto reqister_request{::runner::v1::RegisterRequest{}};
    reqister_request.set_name(config.name);
    reqister_request.set_token(config.token);
    reqister_request.set_version(std::string{runner_version});
    for (auto& [env_type, env_config] : config.environments) {
        for (auto& label : env_config.labels) {
            reqister_request.add_labels(std::move(label));
        }
    }
    reqister_request.set_ephemeral(config.ephemeral);
    auto register_response{runner::register_(client, reqister_request)};
    if (!register_response) {
        std::println(std::cerr, "Error");
        return 1;
    }

    state.uuid = register_response->runner().uuid();
    state.token = register_response->runner().token();

    if (auto res{state.save()}; !res) {
        std::println(std::cerr, "Unable to save state");
        return 1;
    }

    return 0;
}

int cmd_daemon(config::runner_config config, runtime_state state) noexcept {
    using namespace std::literals;

    auto header_source{std::make_shared<class header_source>()};
    http_client client{config.instance_url, header_source};

    header_source->set_uuid(state.uuid);
    header_source->set_token(state.token);

    auto declare_request{::runner::v1::DeclareRequest{}};
    declare_request.set_version(std::string{runner_version});
    for (auto& [env_type, env_config] : config.environments) {
        for (auto& label : env_config.labels) {
            declare_request.add_labels(std::move(label));
        }
    }
    auto declare_response{runner::declare(client, declare_request)};
    if (!declare_response) {
        std::println(std::cerr, "Failed to declare runner: {}", declare_response.error().what());
        return 1;
    }

    run_task_loop(client, config);

    return 0;
}

int main(int argc, char* const argv[]) {
    namespace po = boost::program_options;
    using namespace std::literals;

    try {
        po::options_description options_desc{"Options"};
        options_desc.add_options()                                                           //
            ("help", "show help message")                                                    //
            ("config-file", po::value<std::string>()->required(), "configuration file path") //
            ("state-file", po::value<std::string>()->required(), "state file path")          //
            ("command", po::value<std::string>()->required(), "command (register, daemon)");

        po::positional_options_description positional_desc;
        positional_desc.add("command", 1);

        po::variables_map vm;
        po::store(po::command_line_parser{argc, argv}
                      .options(options_desc)       //
                      .positional(positional_desc) //
                      .run(),
                  vm);

        if (vm.contains("help")) {
            std::cout << "Usage: program [options] command\n"
                      << "Commands:\n"
                      << "  register\n"
                      << "    Register the runner.\n"
                      << "  daemon\n"
                      << "    Start taking jobs.\n"
                      << options_desc << '\n';
            return 1;
        }

        po::notify(vm);

        const program_options options{
            .config_file = std::filesystem::u8path(vm.at("config-file").as<std::string>()),
            .state_file = std::filesystem::u8path(vm.at("state-file").as<std::string>()),
        };

        auto config{config::load_file(options.config_file)};
        if (!config) {
            throw config.error();
        }

        const auto& cmd{vm.at("command").as<std::string>()};
        auto state{runtime_state::load_file(options.state_file)};

        if (cmd == "register"sv) {
            if (!state) {
                state = runtime_state::create(options.state_file);
                if (!state) {
                    throw config.error();
                }
            }
            return cmd_register(std::move(*config), std::move(*state));
        } else if (cmd == "daemon"sv) {
            if (!state) {
                throw config.error();
            }
            return cmd_daemon(std::move(*config), std::move(*state));
        } else {
            std::println(std::cerr, "Invalid command");
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::println(std::cerr, "Error: {}", ex.what());
        return 1;
    }
}

} // namespace ls_gitea_runner

int main(int argc, char* const argv[]) { return ls_gitea_runner::main(argc, argv); }
