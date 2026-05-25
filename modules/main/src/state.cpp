#include "state.hpp"

#include <utility/algorithm.hpp>
#include <utility/string.hpp>

#include <boost/json.hpp>

#include <fstream>
#include <sstream>

namespace ls_gitea_runner {

std::expected<void, GenericError> RuntimeState::save() {
    try {
        std::ostringstream oss{std::ios_base::binary};
        boost::json::object o = {
            {"uuid", uuid},
            {"token", token},
        };
        oss << boost::json::serialize(o);
        std::ofstream ofs{m_file_path, std::ios_base::binary};
        if (!ofs.is_open()) {
            return std::unexpected{GenericError{std::format("Unable to open file for writing: {}",
                                                            utility::string_from_u8string(m_file_path.u8string()))}};
        }
        ofs << oss.str();
        return {};
    } catch (const std::exception& ex) {
        return std::unexpected{GenericError{std::format(
            "Failed to save state file \"{}\": {}", utility::string_from_u8string(m_file_path.u8string()), ex.what())}};
    }
}

std::expected<RuntimeState, GenericError> RuntimeState::load_file(const std::filesystem::path& file_path) {
    try {
        std::ifstream is{file_path, std::ios_base::binary};
        if (!is.is_open()) {
            return std::unexpected{GenericError{std::format("Unable to open file for reading: {}",
                                                            utility::string_from_u8string(file_path.u8string()))}};
        }
        const auto json{boost::json::parse(is).as_object()};
        RuntimeState result{file_path};
        result.uuid = std::string{json.at("uuid").as_string()};
        result.token = std::string{json.at("token").as_string()};
        return result;
    } catch (const std::exception& ex) {
        return std::unexpected{GenericError{std::format(
            "Failed to load state file \"{}\": {}", utility::string_from_u8string(file_path.u8string()), ex.what())}};
    }
}

std::expected<RuntimeState, GenericError> RuntimeState::create(const std::filesystem::path& file_path) {
    RuntimeState result{file_path};
    return result;
}

RuntimeState::RuntimeState(const std::filesystem::path& file_path) : m_file_path{file_path} {}

} // namespace ls_gitea_runner
