#include "libvirt.hpp"

#include "utility/defer.hpp"
#include "utility/encoding/base64.hpp"
#include "utility/spawn.hpp"
#include "utility/string.hpp"
#include "utility/uuid.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <boost/json.hpp>
#include <libvirt/libvirt-qemu.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#include <format>

namespace ls_gitea_runner::libvirt {

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

std::string expand_libvirt_xml_template(std::string_view xml,
                                        const std::unordered_map<std::string_view, std::string>& params) {
    std::string result{xml};
    for (auto& entry : params) {
        const auto pattern{std::format("${{{}}}", entry.first)};
        result = utility::string_replace(result, pattern, entry.second);
    }
    return result;
}

enum class RunLoopState { stopped, starting, running, stopping };

class ConnectionImpl {
public:
    ConnectionImpl(std::string uri) noexcept : m_uri{std::move(uri)} {}

    ~ConnectionImpl() {
        std::scoped_lock lock{*m_mutex};
        if (m_conn) {
            unregister_event_handlers();
        }
    }

    ConnectionImpl(const ConnectionImpl&) = delete;
    ConnectionImpl& operator=(const ConnectionImpl&) = delete;

    ConnectionImpl(ConnectionImpl&&) noexcept = default;
    ConnectionImpl& operator=(ConnectionImpl&&) noexcept = default;

    std::expected<virConnectPtr, GenericError> get() noexcept {
        std::scoped_lock lock{*m_mutex};
        if (m_conn) {
            return m_conn.get();
        }
        return connect(m_uri).and_then([this](auto conn) {
            return register_event_handlers(*this, conn).transform([&] {
                m_conn = std::move(conn);
                return m_conn.get();
            });
        });
    }

private:
    static std::expected<ConnectPtr, GenericError> connect(const std::string& uri) noexcept {
        ConnectPtr conn{virConnectOpen(uri.c_str())};
        if (!conn) {
            return std::unexpected{GenericError{"Failed to connect to hypervisor"}};
        }
        return conn;
    }

    static std::expected<void, GenericError> register_event_handlers(ConnectionImpl& self, ConnectPtr& conn) {
        if (virConnectRegisterCloseCallback(conn.get(), &ConnectionImpl::close_event_cb_internal, &self, nullptr) < 0) {
            return std::unexpected{GenericError{"Failed to register connection closed event handler"}};
        }
        return {};
    }

    void unregister_event_handlers() {
        virConnectUnregisterCloseCallback(m_conn.get(), &ConnectionImpl::close_event_cb_internal);
    }

    static void close_event_cb_internal(virConnectPtr conn, int reason, void* user_data) {
        auto* self{static_cast<ConnectionImpl*>(user_data)};
        self->close_event_handler(conn, reason);
    }

    void close_event_handler(virConnectPtr conn, int reason) {
        std::scoped_lock lock{*m_mutex};
        m_conn.reset();
    }

    std::string m_uri;
    ConnectPtr m_conn;
    std::unique_ptr<std::mutex> m_mutex{std::make_unique<std::mutex>()};
};

class MachineImpl {
public:
    MachineImpl(std::shared_ptr<ConnectionImpl> conn, std::string domain_name, std::string volume_id)
            : m_conn{std::move(conn)}, m_domain_name{std::move(domain_name)}, m_volume_id{std::move(volume_id)} {}

    ~MachineImpl() {
        if (auto volume{get_volume()}) {
            // const std::string name{virStorageVolGetName(volume.get())};
            if (virStorageVolDelete(volume->get(), VIR_STORAGE_VOL_DELETE_NORMAL) < 0) {
                // logger.error("Failed to delete volume \"{}\".", name);
            }
        }
    }

    MachineImpl(const MachineImpl&) = delete;
    MachineImpl(MachineImpl&&) = delete;

    MachineImpl& operator=(const MachineImpl&) = delete;
    MachineImpl& operator=(MachineImpl&&) = delete;

    std::string get_name() const noexcept { return m_domain_name; }

    std::expected<StorageVolPtr, GenericError> get_volume() noexcept {
        return m_conn->get().and_then([this](auto conn_ptr) -> std::expected<StorageVolPtr, GenericError> {
            if (auto volume_ptr{virStorageVolLookupByKey(conn_ptr, m_volume_id.c_str())}) {
                return StorageVolPtr{volume_ptr};
            }
            return std::unexpected{GenericError{std::format("Unable to find volume by UUID: {}", m_volume_id)}};
        });
    }

    std::expected<DomainPtr, GenericError> get_domain() noexcept {
        return m_conn->get().and_then([this](auto conn_ptr) -> std::expected<DomainPtr, GenericError> {
            if (auto volume_ptr{virDomainLookupByName(conn_ptr, m_domain_name.c_str())}) {
                return DomainPtr{volume_ptr};
            }
            return std::unexpected{GenericError{std::format("Unable to find domain by name: {}", m_domain_name)}};
        });
    }

    std::expected<void, GenericError> wait() noexcept {
        std::unique_lock lock{m_mutex};
        m_cv.wait(lock, [&] -> bool { return m_quit; });
        return {};
    }

    std::expected<void, GenericError> wait_for_guest_agent() noexcept {
        std::unique_lock lock{m_mutex};
        if (m_ready) {
            return {};
        }
        m_cv.wait(lock, [&] -> bool { return m_quit || m_ready; });
        if (m_quit) {
            return std::unexpected{
                GenericError{std::format("Domain \"{}\" decayed while waiting for guest agent", m_domain_name)}};
        }
        return {};
    }

    std::expected<void, GenericError> resume() {
        return get_domain().and_then([this](auto domain) -> std::expected<void, GenericError> {
            if (virDomainResume(domain.get()) < 0) {
                return std::unexpected{GenericError{std::format("Failed to resume domain \"{}\"", m_domain_name)}};
            }
            return {};
        });
    }

    std::expected<void, GenericError> kill() {
        return get_domain().and_then([this](auto domain) -> std::expected<void, GenericError> {
            if (virDomainDestroy(domain.get()) < 0) {
                return std::unexpected{GenericError{std::format("Failed to kill domain \"{}\"", m_domain_name)}};
            }
            return {};
        });
    }

    std::expected<bool, GenericError> is_ready() const noexcept {
        if (m_quit) {
            return std::unexpected{GenericError{std::format("Domain \"{}\" has decayed.", m_domain_name)}};
        }
        return m_ready;
    }

    std::expected<void, GenericError> write_file(const std::string& file_path, std::span<const std::byte> content) {
        std::optional<int> file_handle;
        std::optional<GenericError> error;
        utility::Deferred file_closer{[&] {
            if (!file_handle) {
                return;
            }
            auto domain{get_domain()};
            if (!domain) {
                return;
            }
            try {
                const auto req{boost::json::serialize(boost::json::value{{"execute", "guest-file-close"},
                                                                         {
                                                                             "arguments",
                                                                             {
                                                                                 {"handle", *file_handle},
                                                                             },
                                                                         }})};
                auto* res{
                    virDomainQemuAgentCommand(domain->get(), req.c_str(), VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0)};
                if (!res) {
                    error = std::make_optional<GenericError>("Failed to close file on machine");
                }
            } catch (const std::exception& ex) {
                error = std::make_optional<GenericError>(
                    std::format("Error while attempting to close file on machine: {}", ex.what()));
            }
        }};
        auto domain{get_domain()};
        if (!domain) {
            return std::unexpected{domain.error()};
        }
        try {
            auto req{boost::json::serialize(boost::json::value{{"execute", "guest-file-open"},
                                                               {
                                                                   "arguments",
                                                                   {
                                                                       {"path", file_path},
                                                                       {"mode", "w"},
                                                                   },
                                                               }})};
            auto* res{virDomainQemuAgentCommand(domain->get(), req.c_str(), VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0)};

            if (res) {
                file_handle = std::make_optional(boost::json::parse(res).as_object().at("return").as_int64());
                req = boost::json::serialize(boost::json::value{{"execute", "guest-file-write"},
                                                                {
                                                                    "arguments",
                                                                    {
                                                                        {"handle", *file_handle},
                                                                        {"buf-b64", utility::base64_encode(content)},
                                                                    },
                                                                }});
                res = virDomainQemuAgentCommand(domain->get(), req.c_str(), VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0);
                if (!res) {
                    error = std::make_optional<GenericError>("Failed to write file to machine");
                }
            } else {
                error = std::make_optional<GenericError>("Failed to open file on machine");
            }
        } catch (const std::exception& ex) {
            error = std::make_optional<GenericError>(
                std::format("Error while attempting to write file to machine: {}", ex.what()));
        }
        if (error) {
            return std::unexpected{*std::move(error)};
        }
        return {};
    }

    std::expected<utility::SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd) noexcept {
        using namespace std::chrono_literals;
        auto domain{get_domain()};
        if (!domain) {
            return std::unexpected{domain.error()};
        }
        try {
            const auto& exe{cmd.at(0)};
            boost::json::array args{};
            for (size_t i{1}; i < cmd.size(); ++i) {
                args.emplace_back(cmd[i]);
            }
            auto req{boost::json::serialize(boost::json::value{{"execute", "guest-exec"},
                                                               {
                                                                   "arguments",
                                                                   {
                                                                       {"path", exe},
                                                                       {"arg", args},
                                                                       {"capture-output", "merged"},
                                                                   },
                                                               }})};
            auto* res{virDomainQemuAgentCommand(domain->get(), req.c_str(), VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0)};
            if (!res) {
                return std::unexpected{GenericError{"Failed to execute command in machine"}};
            }

            const auto pid{boost::json::parse(res).as_object().at("return").as_object().at("pid").as_int64()};

            bool exited{};
            int exit_code{};
            std::string output;

            while (!exited) {
                req = boost::json::serialize(boost::json::value{{"execute", "guest-exec-status"},
                                                                {
                                                                    "arguments",
                                                                    {
                                                                        {"pid", pid},
                                                                    },
                                                                }});
                res = virDomainQemuAgentCommand(domain->get(), req.c_str(), VIR_DOMAIN_QEMU_AGENT_COMMAND_DEFAULT, 0);
                if (!res) {
                    return std::unexpected{GenericError{"Failed to get command execution status from machine"}};
                }

                const auto status_res{boost::json::parse(res).as_object().at("return").as_object()};
                exited = status_res.at("exited").as_bool();
                if (!exited) {
                    std::this_thread::sleep_for(250ms);
                    continue;
                }

                exit_code = static_cast<int>(status_res.at("exitcode").as_int64());
                output = utility::base64_decode_to_string(status_res.at("out-data").as_string());
            }

            return utility::SpawnResult{.exit_code = exit_code, .output = std::move(output)};
        } catch (const std::exception& ex) {
            return std::unexpected{
                GenericError{std::format("Error while attempting to execute command in machine: {}", ex.what())}};
        }
    }

    std::expected<int, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                utility::SpawnOptions options) noexcept {
        std::abort(); // TODO
    }

    void notify_bad_state() {
        m_quit = true;
        m_cv.notify_all();
    }

    void notify_ready() {
        m_ready = true;
        m_cv.notify_all();
    }

private:
    std::shared_ptr<ConnectionImpl> m_conn;
    std::string m_domain_name;
    StorageVolPtr m_volume;
    std::string m_volume_id;
    std::atomic_bool m_quit{};
    std::atomic_bool m_ready{};
    std::condition_variable m_cv;
    mutable std::mutex m_mutex;
};

Machine::Machine(std::unique_ptr<MachineImpl> impl) : m_impl{std::move(impl)} {}

Machine::~Machine() = default;

Machine::Machine(Machine&&) noexcept = default;
Machine& Machine::operator=(Machine&&) noexcept = default;

std::string Machine::get_name() const noexcept { return m_impl->get_name(); }

std::expected<void, GenericError> Machine::wait() noexcept { return m_impl->wait(); }

std::expected<void, GenericError> Machine::write_file(const std::string& file_path,
                                                      std::span<const std::byte> content) noexcept {
    return m_impl->write_file(file_path, std::move(content));
}

std::expected<utility::SpawnResult, GenericError> Machine::shell_exec(const std::vector<std::string>& cmd) noexcept {
    return m_impl->shell_exec(cmd);
}

std::expected<int, GenericError> Machine::shell_exec(const std::vector<std::string>& cmd,
                                                     utility::SpawnOptions options) noexcept {
    return m_impl->shell_exec(cmd, std::move(options));
}

std::expected<void, GenericError> Machine::resume() noexcept { return m_impl->resume(); }
std::expected<void, GenericError> Machine::kill() noexcept { return m_impl->kill(); }
std::expected<bool, GenericError> Machine::is_ready() const noexcept { return m_impl->is_ready(); }

void Machine::notify_bad_state() { m_impl->notify_bad_state(); }
void Machine::notify_ready() { m_impl->notify_ready(); }

std::expected<void, GenericError> Machine::wait_for_guest_agent() { return m_impl->wait_for_guest_agent(); }

class EventLoopImpl final {
private:
    struct PrivateCtor {};

public:
    EventLoopImpl(PrivateCtor) noexcept {}

    ~EventLoopImpl() {
        stop_loop();
        if (m_loop_thread.joinable()) {
            m_loop_thread.join();
        }
        unregister_event_handlers();
    }

    EventLoopImpl(const EventLoopImpl&) = delete;
    EventLoopImpl(EventLoopImpl&&) = delete;

    EventLoopImpl& operator=(const EventLoopImpl&) = delete;
    EventLoopImpl& operator=(EventLoopImpl&&) = delete;

    static std::expected<std::shared_ptr<EventLoopImpl>, GenericError> get() noexcept {
        // Use weak pointer to allow the shared instance to be deleted sooner than app termination
        static std::weak_ptr<EventLoopImpl> weak_instance;
        static std::mutex m;
        std::scoped_lock lock{m};
        if (auto instance{weak_instance.lock()}) {
            return instance;
        }
        auto create_res{create()};
        if (create_res) {
            weak_instance = *create_res;
        }
        return create_res;
    }

private:
    static std::expected<std::shared_ptr<EventLoopImpl>, GenericError> create() noexcept {
        static std::once_flag initialized;
        std::call_once(initialized, [] {
            virInitialize();
            virEventRegisterDefaultImpl();
        });

        std::shared_ptr<EventLoopImpl> impl;
        try {
            impl = std::make_shared<EventLoopImpl>(PrivateCtor{});
        } catch (const std::bad_alloc& ex) {
            return std::unexpected{GenericError{ex.what()}};
        }

        auto reg_res{impl->register_event_handlers()};
        if (!reg_res) {
            return std::unexpected{reg_res.error()};
        }

        auto start_res{impl->start()};
        if (!start_res) {
            return std::unexpected{start_res.error()};
        }

        return impl;
    }

    std::expected<void, GenericError> start() noexcept {
        {
            std::scoped_lock lock{m_run_loop_state_mutex};
            m_run_loop_state = RunLoopState::starting;
        }
        try {
            m_loop_thread = std::jthread{[this] { run_loop(); }};
            return {};
        } catch (const std::exception& ex) {
            std::scoped_lock lock{m_run_loop_state_mutex};
            m_run_loop_state = RunLoopState::stopped;
            return std::unexpected{GenericError{std::format("Failed to start run loop thread: {}", ex.what())}};
        }
    }

    std::expected<void, GenericError> register_event_handlers() {
        if (const auto id{virEventAddTimeout(1000, +[](int timer, void* opaque) {}, nullptr, nullptr)}; id < 0) {
            return std::unexpected{GenericError{"Unable to register libvirt timeout event needed to stop run loop"}};
        } else {
            m_stop_event_id = id;
        }
        return {};
    }

    void unregister_event_handlers() {
        if (const auto id{m_stop_event_id.exchange(-1)}; id >= 0) {
            virEventRemoveTimeout(id);
        }
    }

    void run_loop() noexcept {
        {
            std::scoped_lock lock{m_run_loop_state_mutex};
            if (m_run_loop_state != RunLoopState::starting) {
                std::abort();
            }
            m_run_loop_state = RunLoopState::running;
        }
        m_start_cv.notify_all();
        while (true) {
            {
                std::scoped_lock lock{m_run_loop_state_mutex};
                if (m_run_loop_state != RunLoopState::running) {
                    break;
                }
            }
            if (virEventRunDefaultImpl() < 0) {
                break;
            }
        }
        {
            std::scoped_lock lock{m_run_loop_state_mutex};
            m_run_loop_state = RunLoopState::stopped;
        }
        m_stop_cv.notify_all();
    }

    void stop_loop() noexcept {
        using namespace std::chrono_literals;
        {
            std::unique_lock lock{m_run_loop_state_mutex};
            m_start_cv.wait(lock, [this] { return m_run_loop_state != RunLoopState::starting; });
            if (m_run_loop_state == RunLoopState::stopped) {
                return;
            }
            m_run_loop_state = RunLoopState::stopping;
        }
        std::unique_lock lock{m_run_loop_state_mutex};
        m_stop_cv.wait(lock, [this] { return m_run_loop_state == RunLoopState::stopped; });
    }

    RunLoopState m_run_loop_state{RunLoopState::stopped};
    std::atomic<int> m_stop_event_id{-1};
    std::condition_variable m_start_cv;
    std::condition_variable m_stop_cv;
    mutable std::mutex m_run_loop_state_mutex;
    std::jthread m_loop_thread;
};

class HypervisorImpl final {
public:
    HypervisorImpl(std::shared_ptr<ConnectionImpl> conn, std::shared_ptr<EventLoopImpl> loop) noexcept
            : m_loop{std::move(loop)}, m_conn{std::move(conn)} {}

    ~HypervisorImpl() { unregister_event_handlers(); }

    HypervisorImpl(const HypervisorImpl&) = delete;
    HypervisorImpl(HypervisorImpl&&) = delete;

    HypervisorImpl& operator=(const HypervisorImpl&) = delete;
    HypervisorImpl& operator=(HypervisorImpl&&) = delete;

    std::expected<std::shared_ptr<Machine>, GenericError> spawn(SpawnOptions options) noexcept {
        const auto conn_res{m_conn->get()};
        if (!conn_res) {
            return std::unexpected{conn_res.error()};
        }
        auto* conn{*conn_res};

        const auto domain_name{utility::uuid()};

        std::unordered_map<std::string_view, std::string> xml_template_params = {
            {"DOMAIN_NAME", domain_name},
        };

        const std::string storage_pool_name{options.storage_pool};
        const auto volume_xml{expand_libvirt_xml_template(options.volume, xml_template_params)};
        auto volume{create_volume(volume_xml, storage_pool_name)};
        if (!volume) {
            return std::unexpected{volume.error()};
        }
        auto volume_id{[&] -> std::expected<std::string, GenericError> {
            const auto* key{virStorageVolGetKey(volume->get())};
            if (!key) {
                return std::unexpected{GenericError{"Unable to get storage volume key"}};
            }
            return key;
        }()};
        if (!volume_id) {
            return std::unexpected{volume_id.error()};
        }

        const auto domain_xml{expand_libvirt_xml_template(options.domain, xml_template_params)};

        auto domain_create_flags{VIR_DOMAIN_START_PAUSED | VIR_DOMAIN_START_RESET_NVRAM};
        DomainPtr domain{virDomainCreateXML(conn, domain_xml.c_str(), domain_create_flags)};
        if (!domain) {
            return std::unexpected{GenericError{std::format("Failed to create domain \"{}\"", domain_name)}};
        }

        for (const auto lifecycle :
             {VIR_DOMAIN_LIFECYCLE_POWEROFF, VIR_DOMAIN_LIFECYCLE_REBOOT, VIR_DOMAIN_LIFECYCLE_CRASH}) {
            const auto action{VIR_DOMAIN_LIFECYCLE_ACTION_DESTROY};
            if (virDomainSetLifecycleAction(domain.get(), lifecycle, action, VIR_DOMAIN_AFFECT_CURRENT) < 0) {
                return std::unexpected{
                    GenericError{std::format("Failed to set lifecycle {} action {} for domain \"{}\"",
                                             static_cast<int>(lifecycle), static_cast<int>(action), domain_name)}};
            }
        }

        std::shared_ptr<Machine> machine;
        {
            std::scoped_lock lock{m_map_mutex};
            auto machine_{m_machine_by_domain_name.emplace(
                domain_name,
                std::make_shared<Machine>(std::make_unique<MachineImpl>(m_conn, domain_name, *std::move(volume_id))))};
            machine = machine_.first->second;
        }

        auto resume_res{machine->resume()};
        if (!resume_res) {
            return std::unexpected{resume_res.error()};
        }

        return machine;
    }

    static std::expected<std::unique_ptr<HypervisorImpl>, GenericError> create(const std::string& uri) noexcept {
        auto loop_res{EventLoopImpl::get()};
        if (!loop_res) {
            return std::unexpected{loop_res.error()};
        }

        std::unique_ptr<HypervisorImpl> impl;
        try {
            impl = std::make_unique<HypervisorImpl>(std::make_shared<ConnectionImpl>(uri), *std::move(loop_res));
        } catch (const std::bad_alloc& ex) {
            return std::unexpected{GenericError{ex.what()}};
        }

        auto reg_res{impl->register_event_handlers()};
        if (!reg_res) {
            return std::unexpected{reg_res.error()};
        }

        return impl;
    }

private:
    std::expected<StorageVolPtr, GenericError> create_volume(const std::string& volume_xml,
                                                             const std::string& pool_name) noexcept {
        const auto conn_res{m_conn->get()};
        if (!conn_res) {
            return std::unexpected{conn_res.error()};
        }
        auto* conn{*conn_res};

        StoragePoolPtr pool{virStoragePoolLookupByName(conn, pool_name.c_str())};
        if (!pool) {
            return std::unexpected{GenericError{std::format("Failed to find storage pool by name \"{}\"", pool_name)}};
        }

        StorageVolPtr volume{virStorageVolCreateXML(pool.get(), volume_xml.c_str(), 0)};
        if (!volume) {
            auto err{virGetLastError()};
            if (err && err->code == VIR_ERR_STORAGE_VOL_EXIST) {
                virStoragePoolRefresh(pool.get(), 0);
                volume = StorageVolPtr{virStorageVolCreateXML(pool.get(), volume_xml.c_str(), 0)};
            }
        }
        if (!volume) {
            return std::unexpected{GenericError{"Failed to create volume"}};
        }
        return volume;
    }

    std::expected<void, GenericError> register_event_handlers() noexcept {
        const auto conn_res{m_conn->get()};
        if (!conn_res) {
            return std::unexpected{conn_res.error()};
        }
        auto* conn{*conn_res};

        static auto lifecycle_event_cb{
            +[](virConnectPtr conn, virDomainPtr dom, int event, int detail, HypervisorImpl* user_data) {
                return user_data->lifecycle_event_handler(conn, dom, event, detail);
            }};

        auto domain_lifecycle_cb_id{virConnectDomainEventRegisterAny(conn, nullptr, VIR_DOMAIN_EVENT_ID_LIFECYCLE,
                                                                     VIR_DOMAIN_EVENT_CALLBACK(lifecycle_event_cb),
                                                                     this, nullptr)};
        if (domain_lifecycle_cb_id < 0) {
            return std::unexpected{GenericError{"Failed to register domain lifecycle event handler"}};
        }
        m_event_handler_ids.push_back(domain_lifecycle_cb_id);

        static auto agent_lifecycle_event_cb{
            +[](virConnectPtr conn, virDomainPtr dom, int state, int reason, HypervisorImpl* user_data) {
                return user_data->agent_lifecycle_event_handler(conn, dom, state, reason);
            }};

        auto agent_lifecycle_cb_id{virConnectDomainEventRegisterAny(conn, nullptr, VIR_DOMAIN_EVENT_ID_AGENT_LIFECYCLE,
                                                                    VIR_DOMAIN_EVENT_CALLBACK(agent_lifecycle_event_cb),
                                                                    this, nullptr)};
        if (agent_lifecycle_cb_id < 0) {
            return std::unexpected{GenericError{"Failed to register agent lifecycle event handler"}};
        }
        m_event_handler_ids.push_back(agent_lifecycle_cb_id);

        return {};
    }

    void unregister_event_handlers() noexcept {
        for (auto it{m_event_handler_ids.rbegin()}; it != m_event_handler_ids.rend(); ++it) {
            if (auto conn{m_conn->get()}) {
                virConnectDomainEventDeregisterAny(*conn, *it);
            }
        }
    }

    int lifecycle_event_handler(virConnectPtr conn, virDomainPtr dom, int event, int detail) noexcept {
        if (auto* domain_name{virDomainGetName(dom)}) {
            if (event == VIR_DOMAIN_EVENT_STARTED || event == VIR_DOMAIN_EVENT_RESUMED) {
                return 0;
            }
            std::scoped_lock lock{m_map_mutex};
            if (auto machine{find_tracked_machine_by_name(domain_name)}) {
                machine->notify_bad_state();
                m_machine_by_domain_name.erase(domain_name);
            }
        }
        return 0;
    }

    int agent_lifecycle_event_handler(virConnectPtr conn, virDomainPtr dom, int state, int reason) noexcept {
        if (auto* domain_name{virDomainGetName(dom)}) {
            if (state != VIR_CONNECT_DOMAIN_EVENT_AGENT_LIFECYCLE_STATE_CONNECTED) {
                return 0;
            }
            std::scoped_lock lock{m_map_mutex};
            if (auto machine{find_tracked_machine_by_name(domain_name)}) {
                machine->notify_ready();
            }
        }
        return 0;
    }

    std::shared_ptr<Machine> find_tracked_machine_by_name(const std::string& name) const noexcept {
        auto found{m_machine_by_domain_name.find(name)};
        if (found != m_machine_by_domain_name.end()) {
            return found->second;
        }
        return {};
    }

    std::shared_ptr<EventLoopImpl> m_loop;
    std::shared_ptr<ConnectionImpl> m_conn;
    std::unordered_map<std::string, std::shared_ptr<Machine>> m_machine_by_domain_name;
    mutable std::mutex m_map_mutex;
    std::vector<int> m_event_handler_ids;
};

Hypervisor::~Hypervisor() = default;

Hypervisor::Hypervisor(Hypervisor&&) noexcept = default;
Hypervisor& Hypervisor::operator=(Hypervisor&&) noexcept = default;

std::expected<std::shared_ptr<Machine>, GenericError> Hypervisor::spawn(SpawnOptions options) noexcept {
    return m_impl->spawn(std::move(options));
}

std::expected<Hypervisor, GenericError> Hypervisor::connect(const std::string& uri) noexcept {
    return HypervisorImpl::create(uri).transform([](auto impl) { return Hypervisor{std::move(impl)}; });
}

Hypervisor::Hypervisor(std::unique_ptr<HypervisorImpl> impl) : m_impl{std::move(impl)} {}

} // namespace ls_gitea_runner::libvirt
