#include "utility/spawn.hpp"
#include "utility/file_handle.hpp"

#ifdef __linux__
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <format>
#include <optional>
#include <span>
#include <sstream>
#include <utility>

namespace ls_gitea_runner::utility {

std::string spawn_escape_arg(const std::string_view arg) {
    // TODO: what about args that start with e.g. "-"?
    if (arg.empty()) {
        return "''";
    }
    std::string result;
    result.reserve(arg.size());
    bool need_quotes{};
    for (char c : arg) {
        switch (c) {
        case '|':
        case '&':
        case ';':
        case '<':
        case '>':
        case '(':
        case ')':
        case '$':
        case '`':
        case '\\':
        case '"':
        case ' ':
        case '\t':
        case '\n':
            need_quotes = true;
            break;
        case '\'':
            need_quotes = true;
            result += '\\';
            break;
        }
        result += c;
    }
    if (need_quotes) {
        return '\'' + result + '\'';
    }
    return result;
}

class fd_wrapper {
public:
    fd_wrapper(int fd) : m_value{fd} {}
    ~fd_wrapper() { close(); }
    fd_wrapper(const fd_wrapper&) = delete;
    fd_wrapper& operator=(const fd_wrapper&) = delete;
    fd_wrapper(fd_wrapper&& other) noexcept { *this = std::move(other); }

    fd_wrapper& operator=(fd_wrapper&& other) noexcept {
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

struct pipefd_wrapper {
    fd_wrapper readable;
    fd_wrapper writable;

    static std::optional<pipefd_wrapper> pipe() {
        std::array<int, 2> pipefd{};
        if (::pipe(pipefd.data()) == 0) {
            return pipefd_wrapper{.readable = pipefd[0], .writable = pipefd[1]};
        }
        return std::nullopt;
    }
};

std::expected<int, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd, spawn_options options) {
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
    auto pipe_to_child{pipefd_wrapper::pipe()};
    if (!pipe_to_child) {
        return std::unexpected{std::runtime_error{"pipe() failed when spawning child process"}};
    }
    auto pipe_from_child{pipefd_wrapper::pipe()};
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
        int wait_status{};
        int exit_code{};
        if (::waitpid(child_pid, &wait_status, WEXITED) != -1) {
            exit_code = WEXITSTATUS(wait_status);
        }
        return exit_code;
    }
#else
    #error Not yet implemented
#endif
}

std::expected<spawn_result, std::runtime_error> spawn_cmd(const std::string cmd) {
    file_handle fp{::popen(cmd.c_str(), "r")};
    if (!fp.get_native_handle()) {
        return std::unexpected{std::runtime_error{std::format("Unable to spawn process with cmd: {}", cmd)}};
    }
    std::ostringstream output;
    std::array<char, 512> buffer{};
    while (true) {
        auto bytes_read{::fread(buffer.data(), 1, buffer.size(), fp.get_native_handle())};
        if (::ferror(fp.get_native_handle())) {
            break;
        }
        if (bytes_read > 0) {
            output.write(buffer.data(), bytes_read);
        }
        if (::feof(fp.get_native_handle())) {
            break;
        }
    }
    auto exit_code{::pclose(fp.get_native_handle())};
    if (exit_code != 0) {
        return std::unexpected{std::runtime_error{std::format("Spawned process returned {}: {}", exit_code, cmd)}};
    }
    return spawn_result{.exit_code = exit_code, .output = output.str()};
}

std::expected<spawn_result, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd) {
    spawn_result result;
    spawn_options options{.stdout_reader = [&](const char* buffer, int length) {
        result.output.append(buffer, static_cast<size_t>(length));
        return length;
    }};
    if (auto res{spawn_cmd(cmd, std::move(options))}) {
        result.exit_code = *res;
        return result;
    } else {
        return std::unexpected{res.error()};
    }
    /*
    if (cmd.empty()) {
        return std::unexpected{std::runtime_error{"Cannot spawn empty command"}};
    }
    std::string cmd_line{cmd[0]};
    for (std::size_t i{1}; i < cmd.size(); ++i) {
        cmd_line += ' ';
        cmd_line += spawn_escape_arg(cmd[i]);
    }
    return spawn_cmd(cmd_line);
    */
}

} // namespace ls_gitea_runner::utility
