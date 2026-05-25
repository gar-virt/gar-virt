#pragma once

#include "error.hpp"

#include <expected>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ls_gitea_runner::scripting {

class ExpressionEvaluator final {
public:
    using value_t = std::variant<std::string, bool, double, std::nullptr_t>;

    ExpressionEvaluator(const std::vector<std::pair<std::string, std::string>>& global_objects);
    ~ExpressionEvaluator();
    std::expected<value_t, GenericError> eval(const std::string& expr);
    std::expected<bool, GenericError> eval_true(const std::string& expr);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

struct ApplyStringSubstitutionsVisitor {
    ApplyStringSubstitutionsVisitor(std::string& result);

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
