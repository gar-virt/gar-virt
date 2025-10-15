#pragma once

#include <boost/json.hpp>
#include <google/protobuf/struct.pb.h>
#include <google/protobuf/timestamp.pb.h>

namespace ls_gitea_runner::protobuf {

boost::json::value protobuf_to_json(const ::google::protobuf::Value& from);
boost::json::value protobuf_to_json(const ::google::protobuf::Struct& from);
boost::json::value protobuf_to_json(const ::google::protobuf::ListValue& from);
boost::json::value protobuf_to_json(const ::google::protobuf::Map<std::string, std::string>& from);

google::protobuf::Timestamp current_timestamp();

} // namespace ls_gitea_runner::protobuf
