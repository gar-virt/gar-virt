#include "commands/commands.hpp"
#include "config.hpp"
#include "program_options.hpp"

#include <utility/log/global_logger.hpp>
#include <utility/string.hpp>

#include <boost/program_options.hpp>

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
            return 0;
        }

        po::notify(vm);

        if (const auto verbose{vm.at("verbose").as<bool>()}) {
            global_logger().set_level(utility::LogLevel::verbose);
        }

        const ProgramOptions options{
            .config_file = utility::u8string_from_string(vm.at("config-file").as<std::string>()),
        };

        const auto config{config::load_file(options.config_file)};
        if (!config) {
            throw config.error();
        }

        auto cmd_res{cmd_daemon(*std::move(config))};
        if (!cmd_res) {
            throw cmd_res.error();
        }

        return 0;
    } catch (const std::exception& ex) {
        std::println(std::cerr, "Error: {}", ex.what());
        return 1;
    }
}

} // namespace ls_gitea_runner

int main(int argc, char* const argv[]) { return ls_gitea_runner::main(argc, argv); }
