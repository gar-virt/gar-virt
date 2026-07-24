export module app:config;

import utility.fs;
import utility.log;
import utility.misc;
import virt;

import std;

namespace ls_gitea_runner::config {

export struct MachineTemplateConfig {
    std::string os;
    Arch::Type arch{};
    std::string temp_dir;
    size_t idle_target{};
    size_t max_concurrency{};
    std::string runner_exe_path;
    std::vector<std::string> labels;
    std::string raw_details;

    std::vector<std::string> get_label_names() const {
        std::vector<std::string> items;
        for (auto label : labels) {
            auto pos{label.find_first_of(':')};
            if (pos != std::string::npos) {
                label = label.substr(0, pos);
            }
            items.push_back(std::move(label));
        }
        return items;
    }
};

export struct ForgeTokenConfig {
    std::string source; // env, file, inline?
    std::string value;
    std::string resolved_token;

    void resolve(const std::filesystem::path& base_dir) {
        if (source == "inline") {
            resolved_token = value;
            return;
        }
        if (source == "env") {
            resolved_token = utility::getenv(value).value_or("");
            return;
        }
        if (source == "file") {
            std::filesystem::path file_path{utility::u8string_from_string(value)};
            if (file_path.is_relative()) {
                file_path = base_dir / file_path;
            }
            auto raw_content{fs::read_file<std::string>(file_path)};
            auto trimmed_content{utility::string_trim(raw_content)};
            resolved_token = raw_content.size() > trimmed_content.size() ? std::string{trimmed_content} : raw_content;
            return;
        }
        throw GenericError{std::format("Invalid token source: {}", source)};
    }
};

export struct ForgeConfig {
    std::string type;
    std::string uri;
    ForgeTokenConfig token;

    void resolve(const std::filesystem::path& base_dir) { token.resolve(base_dir); }
};

export struct LogConfig {
    utility::LogLevel level;
};

export struct BackendConfig {
    std::string type;
    std::string name;
    std::vector<MachineTemplateConfig> templates;
    std::string raw_details;
};

export struct MainConfig {
    std::filesystem::path base_dir;
    size_t config_version{};
    LogConfig log;
    std::string name;
    ForgeConfig forge;
    std::vector<BackendConfig> backends;

    void resolve(const std::filesystem::path& base_dir) {
        this->base_dir = base_dir;
        forge.resolve(base_dir);
    }
};

export std::expected<MainConfig, GenericError> load_file(const std::filesystem::path& file_path);

} // namespace ls_gitea_runner::config
