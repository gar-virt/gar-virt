module;

#include <boost/program_options.hpp>

module app:app_main;

import :commands;
import :config;

import utility.log;
import utility.misc;

import std;

namespace ls_gitea_runner {

export struct ProgramOptions {
    std::filesystem::path config_file;
};

void app_main(int argc, const char* const* argv) {
    namespace po = boost::program_options;
    using namespace std::literals;

    po::options_description options_desc{"Options"};
    options_desc.add_options()                                                           //
        ("help", "show help message")                                                    //
        ("config-file", po::value<std::string>()->required(), "configuration file path") //
        ("verbose", po::bool_switch(), "Verbose logging");

    po::positional_options_description positional_desc;
    positional_desc.add("command", 1);

    po::variables_map vm;
    po::store(po::command_line_parser{argc, argv}
                  .options(options_desc)       //
                  .positional(positional_desc) //
                  .run(),
              vm);

    if (vm.contains("help")) {
        std::cout << "Usage: program [options]\n" << options_desc << '\n';
        return;
    }

    po::notify(vm);

    bool log_level_overridden{};
    if (vm.at("verbose").as<bool>()) {
        global_logger().set_level(utility::LogLevel::debug);
        global_logger().set_capability(utility::LogCapability::log_thread, true);
        log_level_overridden = true;
    }

    const ProgramOptions options{
        .config_file = utility::u8string_from_string(vm.at("config-file").as<std::string>()),
    };

    auto config{config::load_file(options.config_file)};
    if (!config) {
        throw std::move(config).error();
    }

    if (!log_level_overridden) {
        global_logger().set_level(config->log.level);
        global_logger().set_capability(utility::LogCapability::log_thread,
                                       config->log.level == utility::LogLevel::debug);
    }

    auto cmd_res{cmd_daemon(*std::move(config))};
    if (!cmd_res) {
        throw std::move(cmd_res).error();
    }
}

} // namespace ls_gitea_runner
