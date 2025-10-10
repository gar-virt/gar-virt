#include "ping/v1/services.grpc.pb.h"
#include "runner/v1/services.grpc.pb.h"

#include <boost/json.hpp>
#include <curl/curl.h>
#include <grpc++/grpc++.h>

#include <expected>
#include <memory>
#include <print>

struct register_request {};

#if 0
class ping_service_client {
public:
    ping_service_client(std::shared_ptr<grpc::Channel> channel)
        : m_stub{ping::v1::PingService::NewStub(std::move(channel))} {}

    void ping() {
        std::println("ping()");
        auto req{ping::v1::PingRequest{}};
        auto res{ping::v1::PingResponse{}};
        auto context{grpc::ClientContext{}};
        req.set_data("sl-workstation");
        //context.AddMetadata("scheme", "https");
        //context.AddMetadata("path", "/api/v1/actions");
        //context.AddMetadata("x-runner-uuid", "uuid here");
        //context.AddMetadata("x-runner-token", "token here");
        const auto status{m_stub->Ping(&context, req, &res)};
        if (status.ok()) {
            std::println("OK");
        } else {
            std::println("Error: {}", status.error_message());
        }
    }

private:
    std::unique_ptr<ping::v1::PingService::Stub> m_stub;
};

class runner_service_client {
public:
    runner_service_client(std::shared_ptr<grpc::Channel> channel)
        : m_stub{runner::v1::RunnerService::NewStub(std::move(channel))} {}

    void registerRunner() {
        std::println("register()");
        auto req{runner::v1::RegisterRequest{}};
        auto res{runner::v1::RegisterResponse{}};
        auto context{grpc::ClientContext{}};
        context.AddMetadata("scheme", "https");
        context.AddMetadata("host", "gitea.ork.li");
        context.AddMetadata("user-agent", "custom/0.1");
        //context.AddMetadata("method", "post");
        context.AddMetadata("path", "/api/actions/ping.v1.PingService/Ping");
        //context.AddMetadata("content-type", "application/proto");
        //context.AddMetadata("x-runner-uuid", "uuid here");
        //context.AddMetadata("x-runner-token", "token here");
        const auto status{m_stub->Register(&context, req, &res)};
        if (status.ok()) {
            std::println("OK");
        } else {
            std::println("Error: {}", status.error_message());
        }
    }

private:
    std::unique_ptr<runner::v1::RunnerService::Stub> m_stub;
};
#endif

#if 0
class ping_service_client {
public:
    ping_service_client(CURL* curl_handle)
        : m_curl_handle{curl_handle} {}

    bool ping() {

    }

private:
    CURL* m_curl_handle{};
};
#endif

namespace gitea {

namespace {
size_t write_fn(const void *buffer, size_t size, size_t count,
                std::string *output) {
  output->append(static_cast<const char *>(buffer), size * count);
  return size * count;
}
} // namespace

struct context {
  std::string instance;
};

struct service_error {};

} // namespace gitea

namespace gitea::ping::service {

struct ping_request {
  std::string data;
};

struct ping_response {
  std::string data;
};

std::expected<ping_response, service_error> ping(const context &ctx,
                                                 ping_request req) {
  auto payload_json{boost::json::object{}};
  payload_json["data"] = req.data;
  auto payload{boost::json::serialize(payload_json)};

  auto url{ctx.instance};
  if (!url.ends_with('/')) {
    url += '/';
  }
  url += "api/actions/ping.v1.PingService/Ping";

  std::string response_headers;
  std::string response_body;

  curl_slist *headers{};
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");

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
    return std::unexpected{service_error{}};
  }

  try {
    auto response_body_json = boost::json::parse(response_body);
    return ping_response{
        .data = std::string{response_body_json.at("data").as_string()}};
  } catch (const std::exception &) {
    return std::unexpected{service_error{}};
  }

  return std::unexpected{service_error{}};
}

} // namespace gitea::ping::service

int main() {
#if 0
    grpc::experimental::TlsChannelCredentialsOptions cred_options;
    cred_options.set_verify_server_certs(false);
    auto cred{grpc::experimental::TlsCredentials(std::move(cred_options))};
    //auto cred{grpc::InsecureChannelCredentials()};
    auto channel{grpc::CreateChannel("gitea.ork.li", std::move(cred))};

    auto ping_service{ping_service_client{channel}};
    auto runner_service{runner_service_client{channel}};

    ping_service.ping();

    auto req{ping::v1::PingRequest{}};
    req.set_data("sl-workstation");
    auto req_serialized{req.SerializeAsString()};
#endif

  gitea::context ctx{.instance = "http://192.168.2.23:4000"};
  auto result{gitea::ping::service::ping(
      ctx, gitea::ping::service::ping_request{.data = "sl-workstation"})};
  if (result) {
    std::println("OK");
  } else {
    std::println("Error");
  }

  return 0;
}
