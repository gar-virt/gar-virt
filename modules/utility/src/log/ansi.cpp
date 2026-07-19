#include <utility/log/ansi.hpp>

#include <utility/env.hpp>

#include <cstdio>
#include <format>
#include <functional>
#include <iostream>
#include <string>

#ifdef _WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace ls_gitea_runner::utility::ansi {

class SequenceVisitor {
public:
    SequenceVisitor(std::string& output) : m_output_fn{[&](const std::string& input) { output += input; }} {}
    SequenceVisitor(std::ostream& output) : m_output_fn{[&](const std::string& input) { output << input; }} {}
    void operator()(const Color& c) noexcept { m_output_fn(std::format("\33[38;5;{}m", c.code)); }
    void operator()(const Reset&) noexcept { m_output_fn("\33[m"); }

private:
    std::move_only_function<void(const std::string&)> m_output_fn;
};

void write_escape_sequence(std::ostream& output, const Sequence& seq) noexcept {
    std::visit(SequenceVisitor{output}, seq);
}

void write_escape_sequence(std::string& output, const Sequence& seq) noexcept {
    std::visit(SequenceVisitor{output}, seq);
}

bool is_terminal() noexcept { return ::isatty(::fileno(::stdout)) == 1; }

// TODO: Needs Windows-specific checks
//       Also see how to enable virtual terminal processing in cmd for Windows 10 and later:
//       https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
bool is_esc_seq_supported() noexcept { return is_terminal(); }

bool is_color_undesired() noexcept {
    // https://no-color.org/
    // https://web.archive.org/web/20260616201813/https://no-color.org/
    // https://github.com/jcs/no_color
    const auto no_color{getenv("NO_COLOR")};
    return no_color && !no_color->empty();
}

bool is_color_desired_unconditionally() noexcept {
    // https://force-color.org/
    // https://github.com/donatj/force-color.org
    const auto force_color{getenv("FORCE_COLOR")};
    return force_color && !force_color->empty();
}

bool is_color_supported() noexcept {
    if (is_color_desired_unconditionally()) {
        return true;
    }
    if (is_color_undesired()) {
        return false;
    }
    return is_esc_seq_supported();
}

} // namespace ls_gitea_runner::utility::ansi
