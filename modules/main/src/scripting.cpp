#include "scripting.hpp"
#include "error.hpp"

#include <utility/string.hpp>

#include <mujs.h>

#include <cstddef>
#include <cstring>
#include <format>
#include <print>
#include <regex>
#include <string_view>

namespace ls_gitea_runner::scripting {
namespace builtin_fn {

bool contains(const std::string_view search, const std::string_view item) noexcept { return search.contains(item); }

void register_contains(js_State* jss) {
    js_newcfunction(
        jss,
        +[](js_State* jss_) {
            const auto* arg1{js_tostring(jss_, 1)};
            const auto* arg2{js_tostring(jss_, 2)};
            const auto result{builtin_fn::contains(arg1, arg2)};
            js_pushboolean(jss_, result);
        },
        "contains", 2);
    js_setglobal(jss, "contains");
}

bool endsWith(const std::string_view searchString, const std::string_view searchValue) noexcept {
    return searchString.ends_with(searchValue);
}

void register_endsWith(js_State* jss) {
    js_newcfunction(
        jss,
        +[](js_State* jss_) {
            const auto* arg1{js_tostring(jss_, 1)};
            const auto* arg2{js_tostring(jss_, 2)};
            const auto result{builtin_fn::endsWith(arg1, arg2)};
            js_pushboolean(jss_, result);
        },
        "endsWith", 2);
    js_setglobal(jss, "endsWith");
}

bool startsWith(const std::string_view searchString, const std::string_view searchValue) noexcept {
    return searchString.starts_with(searchValue);
}

void register_startsWith(js_State* jss) {
    js_newcfunction(
        jss,
        +[](js_State* jss_) {
            const auto* arg1{js_tostring(jss_, 1)};
            const auto* arg2{js_tostring(jss_, 2)};
            const auto result{builtin_fn::startsWith(arg1, arg2)};
            js_pushboolean(jss_, result);
        },
        "startsWith", 2);
    js_setglobal(jss, "startsWith");
}

} // namespace builtin_fn

class ExpressionEvaluator::Impl final {
public:
    using value_t = std::variant<std::string, bool, double, std::nullptr_t>;

    Impl(const std::vector<std::pair<std::string, std::string>>& global_objects)
            : m_jss{js_newstate(nullptr, nullptr, JS_STRICT)} {
        js_setreport(
            m_jss,
            +[](js_State* jss_, const char* message) { std::println("Expression evaluation error: {}", message); });
        builtin_fn::register_contains(m_jss);
        builtin_fn::register_endsWith(m_jss);
        builtin_fn::register_startsWith(m_jss);
        add_global_objects(global_objects);
    }

    ~Impl() { js_freestate(m_jss); }

    std::expected<value_t, GenericError> eval(const std::string& expr) {
        if (js_ploadstring(m_jss, "[script]", expr.c_str())) {
            return std::unexpected{GenericError{std::format("Failed to load expression <{}>", expr)}};
        }

        // Push the "this" value
        js_pushglobal(m_jss);

        if (js_pcall(m_jss, 0)) {
            // Pop the "this" value?
            js_pop(m_jss, 1);
            return std::unexpected{GenericError{std::format("Evaluation of expression <{}> failed", expr)}};
        }

        std::expected<value_t, GenericError> eval_result;

        if (js_isboolean(m_jss, -1)) {
            eval_result = js_toboolean(m_jss, -1) != 0;
        } else if (js_isstring(m_jss, -1)) {
            eval_result = std::string{js_tostring(m_jss, -1)};
        } else if (js_isnumber(m_jss, -1)) {
            eval_result = js_tonumber(m_jss, -1);
        } else if (js_isnull(m_jss, -1)) {
            eval_result = nullptr;
        } else {
            return std::unexpected{
                GenericError{std::format("Evaluation of expression <{}> resulted in unsupported type", expr)}};
        }

        // TODO: object, array, NaN?

        // Pop result
        js_pop(m_jss, 1);
        return eval_result;
    }

    std::expected<bool, GenericError> eval_true(const std::string& expr) {
        const auto expr_{std::format("!!({})", expr)};
        return eval(expr_).transform([](auto res) {
            auto bool_res{std::get<bool>(res)};
            return bool_res;
        });
    }

private:
    void add_global_objects(const std::vector<std::pair<std::string, std::string>>& global_objects) {
        using namespace std::literals;
        std::string script;
        for (auto& [k, v] : global_objects) {
            script += std::format("var {} = {}\n", k, v);
        }
        js_dostring(m_jss, script.c_str());
    }

    js_State* m_jss{};
};

ExpressionEvaluator::ExpressionEvaluator(const std::vector<std::pair<std::string, std::string>>& global_objects)
        : m_impl{std::make_unique<Impl>(global_objects)} {}

ExpressionEvaluator::~ExpressionEvaluator() {}

std::expected<ExpressionEvaluator::value_t, GenericError> ExpressionEvaluator::eval(const std::string& expr) {
    return m_impl->eval(expr);
}

std::expected<bool, GenericError> ExpressionEvaluator::eval_true(const std::string& expr) {
    return m_impl->eval_true(expr);
}

ApplyStringSubstitutionsVisitor::ApplyStringSubstitutionsVisitor(std::string& result) : m_result{result} {}

std::string ApplyStringSubstitutionsVisitor::operator()(std::nullptr_t) { return m_result = "null"; }
std::string ApplyStringSubstitutionsVisitor::operator()(bool v) { return m_result = v ? "true" : "false"; }
std::string ApplyStringSubstitutionsVisitor::operator()(double v) { return m_result = std::format("{}", v); }
std::string ApplyStringSubstitutionsVisitor::operator()(const std::string& v) { return m_result = v; }

std::string apply_string_substitutions(std::string script_str,
                                       const std::vector<std::pair<std::string, std::string>>& contexts) {
    static const std::regex pattern{R"re(\$\{\{\s*(.*?)\s*}})re"};
    script_str = utility::regex_replace_callable(script_str, pattern, [&](const std::smatch& m) -> std::string {
        scripting::ExpressionEvaluator expr_eval{contexts};
        const auto& expr{m[1].str()};
        if (auto eval_result{expr_eval.eval(expr)}) {
            std::string result_value;
            std::visit(ApplyStringSubstitutionsVisitor{result_value}, *eval_result);
            return result_value;
        }
        // TODO: report error
        return "null";
    });
    return script_str;
}

} // namespace ls_gitea_runner::scripting
