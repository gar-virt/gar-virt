module;

#include <ping/v1/messages.pb.h>
#include <runner/v1/messages.pb.h>

export module gitea:proto;

export namespace ping::v1 {
using ::ping::v1::PingRequest;
using ::ping::v1::PingResponse;
} // namespace ping::v1

export namespace runner::v1 {
using ::runner::v1::DeclareRequest;
using ::runner::v1::DeclareResponse;
using ::runner::v1::FetchTaskRequest;
using ::runner::v1::FetchTaskResponse;
using ::runner::v1::LogRow;
using ::runner::v1::RegisterRequest;
using ::runner::v1::RegisterResponse;
using ::runner::v1::Result;
using ::runner::v1::Runner;
using ::runner::v1::StepState;
using ::runner::v1::Task;
using ::runner::v1::TaskNeed;
using ::runner::v1::TaskState;
using ::runner::v1::UpdateLogRequest;
using ::runner::v1::UpdateLogResponse;
using ::runner::v1::UpdateTaskRequest;
using ::runner::v1::UpdateTaskResponse;
} // namespace runner::v1
