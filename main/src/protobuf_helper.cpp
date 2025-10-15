#include "protobuf_helper.hpp"

#include "error.hpp"

#include <chrono>

namespace ls_gitea_runner::protobuf {

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

} // namespace ls_gitea_runner::protobuf
