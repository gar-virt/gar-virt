#include <utility/env.hpp>

#include <boost/json.hpp>
#include <curl/curl.h>
#include <grpc++/grpc++.h>

#include <cstring>
#include <expected>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace gitea {

namespace {
auto write_fn(void const *buffer, size_t size, size_t count,
              std::string *output) -> size_t {
  output->append(static_cast<char const *>(buffer), size * count);
  return size * count;
}
} // namespace

struct http_response {
  bool ok{};
  boost::json::value body;
};

class http_header_source {
public:
  virtual auto
  set_headers(std::function<
              auto(std::string const &name, std::string const &value)->void>
                  cb) -> void const = 0;
};

class http_client {
public:
  http_client(std::string const &base_url,
              std::shared_ptr<http_header_source> header_source = {})
      : m_base_url{base_url}, m_header_source{std::move(header_source)} {
    if (!m_base_url.ends_with('/')) {
      m_base_url += '/';
    }
    m_base_url += "api/actions";
  }

  auto post(std::string const &path,
            boost::json::value const &payload_json) const -> http_response {
    auto payload{boost::json::serialize(payload_json)};
    auto const url{m_base_url + path};

    std::string response_headers;
    std::string response_body;

    curl_slist *headers{};
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    m_header_source->set_headers(
        [&](std::string const &name, std::string const &value) {
          std::string header{name};
          header += ": ";
          header += value;
          headers = curl_slist_append(headers, header.c_str());
        });

    CURLcode curl_code{CURLE_OK};
    long response_code{};
    auto *curl{curl_easy_init()};
    if (curl) {
      curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
      curl_easy_setopt(curl, CURLOPT_HEADER, 0);
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
      curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_fn);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_fn);
      curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
      curl_code = curl_easy_perform(curl);
      if (curl_code == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      }
      curl_easy_cleanup(curl);
    }

    curl_slist_free_all(headers);

    if (!curl || curl_code != CURLE_OK) {
      return http_response{.ok = false};
    }

    try {
      return http_response{.ok = true,
                           .body = boost::json::parse(response_body)};
    } catch (std::exception const &) {
      return http_response{.ok = false};
    }

    return http_response{.ok = false};
  }

private:
  std::string m_base_url;
  std::shared_ptr<http_header_source> m_header_source;
};

struct service_error {};

namespace ping {

struct ping_request {
  std::string data;

  auto to_json() -> boost::json::value {
    return boost::json::value{{"data", data}};
  }
};

struct ping_response {
  std::string data;

  static auto from_json(boost::json::value const &v)
      -> std::expected<ping_response, service_error> {
    try {
      auto const &o{v.as_object()};
      return ping_response{.data = std::string{o.at("data").as_string()}};
    } catch (std::exception const &) {
      return std::unexpected{service_error{}};
    }
  }
};

auto ping(http_client const &client, ping_request req)
    -> std::expected<ping_response, service_error> {
  try {
    auto res{client.post("/ping.v1.PingService/Ping", req.to_json())};
    return ping_response::from_json(res.body);
  } catch (std::exception const &) {
    return std::unexpected{service_error{}};
  }

  return std::unexpected{service_error{}};
}

} // namespace ping

namespace runner {

enum class runner_status {
  unspecified = 0,
  idle = 1,
  active = 2,
  offline = 3,
};

struct runner {
  std::int64_t id{};
  std::string uuid;
  std::string token;
  std::string name;
  runner_status status{};
  std::string version;
  std::vector<std::string> labels;
  bool ephemeral{};

  static auto from_json(boost::json::value const &v) -> runner {
    auto const &o{v.as_object()};
    runner r;
    if (o.contains("id")) {
      r.id = std::stoll(std::string{o.at("id").as_string()});
    }
    if (o.contains("uuid")) {
      r.uuid = std::string{o.at("uuid").as_string()};
    }
    if (o.contains("token")) {
      r.token = std::string{o.at("token").as_string()};
    }
    if (o.contains("name")) {
      r.name = std::string{o.at("name").as_string()};
    }
    if (o.contains("status")) {
      r.status = static_cast<runner_status>(o.at("status").as_int64());
    }
    if (o.contains("version")) {
      r.version = std::string{o.at("version").as_string()};
    }
    if (o.contains("labels")) {
      for (auto const &label : o.at("labels").as_array()) {
        r.labels.emplace_back(label.as_string());
      }
    }
    if (o.contains("ephemeral")) {
      r.ephemeral = o.at("ephemeral").as_bool();
    }
    return r;
  }
};

struct task {};

struct declare_request {
  std::string version;
  std::vector<std::string> labels;

  auto to_json() -> boost::json::value {
    return boost::json::object{{"version", version},
                               {"labels", boost::json::value_from(labels)}};
  }
};

struct declare_response {
  struct runner runner;

  static auto from_json(boost::json::value const &v)
      -> std::expected<declare_response, service_error> {
    return declare_response{.runner =
                                runner::from_json(v.at("runner").as_object())};
  }
};

struct register_request {
  std::string name;
  std::string token;
  std::string version;
  std::vector<std::string> labels;
  bool ephemeral{};

  auto to_json() -> boost::json::value {
    return boost::json::object{{"token", token},
                               {"name", name},
                               {"version", version},
                               {"labels", boost::json::value_from(labels)},
                               {"ephemeral", ephemeral}};
  }
};

struct register_response {
  struct runner runner;

  static auto from_json(boost::json::value const &v) -> register_response {
    return register_response{.runner =
                                 runner::from_json(v.at("runner").as_object())};
  }
};

struct fetch_task_request {
  std::int64_t tasks_version{};

  auto to_json() -> boost::json::value {
    return boost::json::object{{"tasks_version", tasks_version}};
  }
};

struct fetch_task_response {
  struct task task;
  std::int64_t tasks_version{};

  static auto from_json(boost::json::value const &v)
      -> std::expected<fetch_task_response, service_error> {
    return fetch_task_response{.tasks_version = std::stoll(std::string{
                                   v.at("tasksVersion").as_string()})};
  }
};

auto register_(http_client const &client, register_request req)
    -> std::expected<register_response, service_error> {
  try {
    auto res{client.post("/runner.v1.RunnerService/Register", req.to_json())};
    return register_response::from_json(res.body);
  } catch (std::exception const &) {
    return std::unexpected{service_error{}};
  }

  return std::unexpected{service_error{}};
}

auto declare(http_client const &client, declare_request req)
    -> std::expected<declare_response, service_error> {
  try {
    auto res{client.post("/runner.v1.RunnerService/Declare", req.to_json())};
    return declare_response::from_json(res.body);
  } catch (std::exception const &) {
    return std::unexpected{service_error{}};
  }

  return std::unexpected{service_error{}};
}

auto fetch_task(http_client const &client, fetch_task_request req)
    -> std::expected<fetch_task_response, service_error> {
  try {
    auto res{client.post("/runner.v1.RunnerService/FetchTask", req.to_json())};
    return fetch_task_response::from_json(res.body);
  } catch (std::exception const &) {
    return std::unexpected{service_error{}};
  }

  return std::unexpected{service_error{}};
}

} // namespace runner

class header_source : public http_header_source {
public:
  auto
  set_headers(std::function<
              auto(std::string const &name, std::string const &value)->void>
                  cb) -> void const {
    if (!m_uuid.empty()) {
      cb("X-Runner-UUID", m_uuid);
    }

    if (!m_token.empty()) {
      cb("X-Runner-Token", m_token);
    }
  }

  void set_uuid(std::string const &uuid) { m_uuid = uuid; }
  void set_token(std::string const &token) { m_token = token; }

private:
  std::string m_uuid;
  std::string m_token;
};

auto parse_runner_labels_env_var(std::string_view const line)
    -> std::vector<std::string> {
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

  static runner_options from_env() {
    runner_options o{
        .instance{utility::getenv("GITEA_INSTANCE_URL")},
        .runner_name{utility::getenv("GITEA_RUNNER_NAME")},
        .runner_registration_token{
            utility::getenv("GITEA_RUNNER_REGISTRATION_TOKEN")},
        .runner_uuid{utility::getenv("GITEA_RUNNER_UUID")},
        .runner_token{utility::getenv("GITEA_RUNNER_TOKEN")},
    };
    if (auto const labels{utility::getenv("GITEA_RUNNER_LABELS")}) {
      o.runner_labels = parse_runner_labels_env_var(*labels);
    }
    return o;
  }
};

} // namespace gitea

constexpr auto runner_version{std::string_view{"v0.1.0"}};

auto cmd_register() -> int {
  // GITEA_RUNNER_EPHEMERAL
  // GITEA_RUNNER_REGISTRATION_TOKEN_FILE

  auto const options{gitea::runner_options::from_env()};

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

  auto header_source{std::make_shared<gitea::header_source>()};
  gitea::http_client client{*options.instance, header_source};

  auto ping_result{gitea::ping::ping(
      client, gitea::ping::ping_request{.data = *options.runner_name})};
  if (!ping_result) {
    std::println(std::cerr, "Error");
    return 1;
  }

  auto reqister_request{gitea::runner::register_request{
      .name = *options.runner_name,
      .token = *options.runner_registration_token,
      .version = std::string{runner_version},
      .labels = options.runner_labels,
      .ephemeral = true,
  }};
  auto register_result{gitea::runner::register_(client, reqister_request)};
  if (!register_result) {
    std::println(std::cerr, "Error");
    return 1;
  }

  std::println("uuid={}", register_result->runner.uuid);
  std::println("token={}", register_result->runner.token);

  return 0;
}

auto cmd_daemon() -> int {
  auto const options{gitea::runner_options::from_env()};

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

  auto header_source{std::make_shared<gitea::header_source>()};
  gitea::http_client client{*options.instance, header_source};

  header_source->set_uuid(*options.runner_uuid);
  header_source->set_token(*options.runner_token);

  auto declare_request{gitea::runner::declare_request{
      .version = std::string{runner_version}, .labels = options.runner_labels}};
  auto declare_result{gitea::runner::declare(client, declare_request)};
  if (!declare_result) {
    std::println(std::cerr, "Error");
    return 1;
  }

  auto fetch_task_request{gitea::runner::fetch_task_request{}};
  auto fetch_task_result{gitea::runner::fetch_task(client, fetch_task_request)};
  if (!fetch_task_result) {
    std::println(std::cerr, "Error");
    return 1;
  }

  return 0;
}

int main(int argc, char *const argv[]) {
  using namespace std::literals;

  if (argc < 2) {
    std::println(std::cerr, "Missing command");
    return 1;
  }

  auto const cmd{std::string_view{argv[1]}};
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
