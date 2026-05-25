#include "utility/spawn.hpp"

#ifdef __linux__
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <optional>
#include <span>
#include <utility>

namespace ls_gitea_runner::utility {

class FdWrapper {
public:
    FdWrapper(int fd) : m_value{fd} {}
    ~FdWrapper() { close(); }
    FdWrapper(const FdWrapper&) = delete;
    FdWrapper& operator=(const FdWrapper&) = delete;
    FdWrapper(FdWrapper&& other) noexcept { *this = std::move(other); }

    FdWrapper& operator=(FdWrapper&& other) noexcept {
        if (this != &other) {
            m_value = std::exchange(other.m_value, 0);
        }
        return *this;
    }

    int value() const { return m_value; }

    void close() {
        if (m_value) {
            ::close(m_value);
            m_value = 0;
        }
    }

    ssize_t read(std::span<char> buffer) const { return ::read(m_value, buffer.data(), buffer.size()); }
    ssize_t write(const std::span<char> buffer) { return ::write(m_value, buffer.data(), buffer.size()); }

private:
    int m_value{};
};

struct PipefdWrapper {
    FdWrapper readable;
    FdWrapper writable;

    static std::optional<PipefdWrapper> pipe() {
        std::array<int, 2> pipefd{};
        if (::pipe(pipefd.data()) == 0) {
            return PipefdWrapper{.readable = pipefd[0], .writable = pipefd[1]};
        }
        return std::nullopt;
    }
};

std::expected<int, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd, SpawnOptions options) {
    if (cmd.empty()) {
        return std::unexpected{std::runtime_error{"Cannot spawn empty command"}};
    }
    if (!options.stdin_writer) {
        options.stdin_writer = [](char*, ssize_t) { return 0; };
    }
    if (!options.stdout_reader) {
        options.stdout_reader = [](const char*, ssize_t) { return 0; };
    }
    const auto argv{[&] {
        std::vector<const char*> result;
        for (auto& arg : cmd) {
            result.push_back(arg.c_str());
        }
        result.push_back(nullptr);
        return result;
    }()};
#ifdef __linux__
    auto pipe_to_child{PipefdWrapper::pipe()};
    if (!pipe_to_child) {
        return std::unexpected{std::runtime_error{"pipe() failed when spawning child process"}};
    }
    auto pipe_from_child{PipefdWrapper::pipe()};
    if (!pipe_from_child) {
        return std::unexpected{std::runtime_error{"pipe() failed when spawning child process"}};
    }
    pid_t child_pid{-1};
    child_pid = ::fork();
    if (child_pid == -1) {
        return std::unexpected{std::runtime_error{"fork() failed when spawning child process"}};
    }
    if (child_pid == 0) {
        // Continuing in the child process
        pipe_to_child->writable.close();
        pipe_from_child->readable.close();
        ::dup2(pipe_to_child->readable.value(), STDIN_FILENO);
        pipe_to_child->readable.close();
        ::dup2(pipe_from_child->writable.value(), STDOUT_FILENO);
        ::dup2(pipe_from_child->writable.value(), STDERR_FILENO);
        pipe_from_child->writable.close();
        ::execvp(argv.at(0), const_cast<char* const*>(argv.data()));
        // Shouldn't reach here, and we can't recover anyway if it happens
        std::abort();
    } else {
        // Continuing in the parent process
        pipe_to_child->readable.close();
        pipe_from_child->writable.close();
        std::array<char, 512> buffer{};
        // Write to child
        while (true) {
            auto amount{options.stdin_writer(buffer.data(), buffer.size())};
            if (amount < 1) {
                pipe_to_child->writable.close();
                break;
            }
            pipe_to_child->writable.write(std::span<char>{buffer.data(), static_cast<std::size_t>(amount)});
        }
        // Read from child
        while (true) {
            auto amount_from_child{pipe_from_child->readable.read(buffer)};
            if (amount_from_child < 1) {
                pipe_from_child->readable.close();
                break;
            }
            options.stdout_reader(buffer.data(), amount_from_child);
        }
        int exit_code{-1};
        siginfo_t si{};
        if (::waitid(P_PID, child_pid, &si, WEXITED) != -1) {
            if (si.si_code == CLD_EXITED) {
                exit_code = si.si_status;
            }
        }
        return exit_code;
    }
#else
    #error Not yet implemented
#endif
}

std::expected<SpawnResult, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd) {
    SpawnResult result;
    SpawnOptions options{.stdout_reader = [&](const char* buffer, int length) {
        result.output.append(buffer, static_cast<size_t>(length));
        return length;
    }};
    if (auto res{spawn_cmd(cmd, std::move(options))}) {
        result.exit_code = *res;
        return result;
    } else {
        return std::unexpected{res.error()};
    }
}

} // namespace ls_gitea_runner::utility
