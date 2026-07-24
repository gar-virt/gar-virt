module;

#include <iostream>
#include <variant>

export module utility.log:stdout_;

import :ansi;
import :log;
import :level;

namespace ls_gitea_runner::utility {
namespace {

struct PrintVisitor {
    constexpr PrintVisitor(std::ostream* os) : os{os} {}
    constexpr void operator()(const ansi::Sequence& /*seq*/) const {}

    constexpr void operator()(const std::string& s) const {
        os->write(s.data(), safe_cast_int<std::streamsize>(s.size()));
    }

    std::ostream* os{};
};

struct ColorPrintVisitor {
    constexpr ColorPrintVisitor(std::ostream* os) : os{os} {}
    constexpr void operator()(const ansi::Sequence& seq) const { ansi::write_escape_sequence(*os, seq); }

    constexpr void operator()(const std::string& s) const {
        os->write(s.data(), safe_cast_int<std::streamsize>(s.size()));
    }

    std::ostream* os{};
};

} // namespace

export class StdOutLogger final : public Logger {
public:
    StdOutLogger() noexcept : m_enable_color{ansi::is_color_supported()} {}

private:
    void print_impl(const LogRequest& req) override {
        std::ostream* file{is_error_like(req.level) ? &std::cerr : &std::cout};
        if (m_enable_color) {
            for (const auto& part : req.line) {
                std::visit(ColorPrintVisitor{file}, part);
            }
        } else {
            for (const auto& part : req.line) {
                std::visit(PrintVisitor{file}, part);
            }
        }
        if (req.always_flush) {
            file->flush();
        }
    }

    bool m_enable_color{};
};

} // namespace ls_gitea_runner::utility
