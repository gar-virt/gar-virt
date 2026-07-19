#pragma once

#include <iosfwd>
#include <string>
#include <variant>

namespace ls_gitea_runner::utility::ansi {

struct Color {
    int code{};
};

struct Reset {};

using Sequence = std::variant<Color, Reset>;

void write_escape_sequence(std::ostream& output, const Sequence& seq);
void write_escape_sequence(std::string& output, const Sequence& seq);

bool is_terminal() noexcept;
bool is_esc_seq_supported() noexcept;
bool is_color_undesired();
bool is_color_desired_unconditionally();
bool is_color_supported();

} // namespace ls_gitea_runner::utility::ansi
