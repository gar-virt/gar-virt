#include "commands/commands.hpp"
#include "config.hpp"
#include "program_options.hpp"
#include "state.hpp"

#include <boost/program_options.hpp>

#include <filesystem>
#include <iostream>
#include <print>
#include <string>

namespace ls_gitea_runner {

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
            return 0;
        }

        po::notify(vm);

        const ProgramOptions options{
            .config_file = std::filesystem::u8path(vm.at("config-file").as<std::string>()),
            .state_file = std::filesystem::u8path(vm.at("state-file").as<std::string>()),
        };

        auto config{config::load_file(options.config_file)};
        if (!config) {
            throw config.error();
        }

        const auto& cmd{vm.at("command").as<std::string>()};
        auto state{RuntimeState::load_file(options.state_file)};

        if (cmd == "register"sv) {
            if (!state) {
                state = RuntimeState::create(options.state_file);
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
