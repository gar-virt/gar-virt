#pragma once

#include "error.hpp"

#include <expected>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ls_gitea_runner::scripting {

class expression_evaluator final {
public:
    using value_t = std::variant<std::string, bool, double, std::nullptr_t>;

    expression_evaluator(const std::vector<std::pair<std::string, std::string>>& global_objects);
    ~expression_evaluator();
    std::expected<value_t, generic_error> eval(const std::string& expr);
    std::expected<bool, generic_error> eval_true(const std::string& expr);

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

struct apply_string_substitutions_visitor {
    apply_string_substitutions_visitor(std::string& result);

    std::string operator()(std::nullptr_t);
    std::string operator()(bool v);
    std::string operator()(double v);
    std::string operator()(const std::string& v);

    // TODO: object, array

private:
    std::string& m_result;
};

std::string apply_string_substitutions(std::string script_str,
                                       const std::vector<std::pair<std::string, std::string>>& contexts);

} // namespace ls_gitea_runner::scripting
