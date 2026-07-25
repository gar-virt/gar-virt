module;

#include <libvirt/libvirt-qemu.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

export module virt:libvirt;

import utility.encoding;
import utility.misc;

import std;

export namespace ls_gitea_runner::libvirt {

struct SpawnOptions {
    std::string volume;
    std::string domain;
    std::string storage_pool;
};

struct SpawnResult {
    int exit_code{};
    std::string output;
};

struct ConnectDeleter {
    void operator()(virConnectPtr p) { virConnectClose(p); }
};

struct StoragePoolDeleter {
    void operator()(virStoragePoolPtr p) { virStoragePoolFree(p); }
};

struct StorageVolDeleter {
    void operator()(virStorageVolPtr p) { virStorageVolFree(p); }
};

struct StorageVolResourceDeleter {
    void operator()(virStorageVolPtr p) { virStorageVolDelete(p, VIR_STORAGE_VOL_DELETE_NORMAL); }
};

struct DomainDeleter {
    void operator()(virDomainPtr p) { virDomainFree(p); }
};

using ConnectPtr = std::unique_ptr<virConnect, ConnectDeleter>;
using StoragePoolPtr = std::unique_ptr<virStoragePool, StoragePoolDeleter>;
using StorageVolPtr = std::unique_ptr<virStorageVol, StorageVolDeleter>;
using DomainPtr = std::unique_ptr<virDomain, DomainDeleter>;

class MachineImpl;

class Machine {
public:
    Machine(std::unique_ptr<MachineImpl> impl) noexcept;
    ~Machine();
    Machine(const Machine&) = delete;
    Machine(Machine&&) noexcept;
    Machine& operator=(const Machine&) = delete;
    Machine& operator=(Machine&&) noexcept;
    const std::string& get_name() const noexcept;
    std::expected<StorageVolPtr, GenericError> get_volume();
    std::expected<DomainPtr, GenericError> get_domain();
    std::expected<void, GenericError> wait();
    std::expected<void, GenericError> wait_for_guest_agent();
    std::expected<void, GenericError> resume();
    std::expected<void, GenericError> kill();
    std::expected<bool, GenericError> is_ready() const;
    std::expected<void, GenericError> write_file(const std::string& file_path, std::span<const std::byte> content);

    std::expected<SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                        const std::optional<std::chrono::seconds>& timeout);

    void notify_bad_state();
    void notify_ready();

private:
    std::unique_ptr<MachineImpl> m_impl;
};

class HypervisorImpl;

class Hypervisor final {
public:
    Hypervisor(std::unique_ptr<HypervisorImpl> impl) noexcept;
    ~Hypervisor();
    Hypervisor(const Hypervisor&) = delete;
    Hypervisor(Hypervisor&&) noexcept;
    Hypervisor& operator=(const Hypervisor&) = delete;
    Hypervisor& operator=(Hypervisor&&) noexcept;
    std::expected<std::shared_ptr<Machine>, GenericError> spawn(const SpawnOptions& options);
    static std::expected<std::shared_ptr<Hypervisor>, GenericError> connect(const std::string& uri);

private:
    std::unique_ptr<HypervisorImpl> m_impl;
};

} // namespace ls_gitea_runner::libvirt
