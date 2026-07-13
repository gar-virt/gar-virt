#include <utility/log/stdout.hpp>

#include <utility/log/ansi.hpp>

#include <iostream>

namespace ls_gitea_runner::utility {
namespace {

struct PrintVisitor {
    constexpr PrintVisitor(std::ostream* os) : os{os} {}
    constexpr void operator()(const ansi::Sequence& seq) noexcept {}
    constexpr void operator()(const std::string& s) noexcept { os->write(s.data(), s.size()); }

    std::ostream* os{};
};

struct ColorPrintVisitor {
    constexpr ColorPrintVisitor(std::ostream* os) : os{os} {}
    constexpr void operator()(const ansi::Sequence& seq) noexcept { ansi::write_escape_sequence(*os, seq); }
    constexpr void operator()(const std::string& s) noexcept { os->write(s.data(), s.size()); }

    std::ostream* os{};
};

} // namespace

StdOutLogger::StdOutLogger() noexcept : m_enable_color{ansi::is_color_supported()} {}

void StdOutLogger::print_impl(const LogRequest& req) noexcept {
    std::ostream* file{is_error_like(req.level) ? &std::cerr : &std::cout};
    if (m_enable_color) {
        for (auto& part : req.line) {
            std::visit(ColorPrintVisitor{file}, part);
        }
    } else {
        for (auto& part : req.line) {
            std::visit(PrintVisitor{file}, part);
        }
    }
    if (req.always_flush) {
        file->flush();
    }
}

} // namespace ls_gitea_runner::utility
