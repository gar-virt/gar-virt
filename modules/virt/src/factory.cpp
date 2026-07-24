export module virt:factory;

import :api;
import :libvirt_backend;

import utility.misc;

import std;

export namespace ls_gitea_runner {

class MachineManagerFactorySelector final {
public:
    static std::expected<std::unique_ptr<MachineManagerFactory>, GenericError> get_factory(const std::string& name) {
        if (name == "libvirt") {
            return std::make_unique<LibvirtMachineManagerFactory>();
        }
        return std::unexpected{GenericError{std::format("Invalid machine manager factory name: {}", name)}};
    }
};

} // namespace ls_gitea_runner
