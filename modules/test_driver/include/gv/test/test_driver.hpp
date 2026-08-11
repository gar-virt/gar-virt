/*
 * Copyright 2024 Steffen André Langnes
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <exception>
#include <functional>
#include <map>
#include <string>

namespace gv::test {

struct failure_info {
    const char* condition;
    const char* file;
    int line;
};

struct test_failure : public std::exception {
    explicit test_failure(const failure_info& info) noexcept : m_info{info} {}
    const char* what() const noexcept override { return "test failure"; }
    const failure_info& info() const noexcept { return m_info; }

private:
    failure_info m_info;
};

class test_reg {
public:
    test_reg() = default;

    test_reg(const char* name, std::function<void()> fn) noexcept : m_name{name}, m_fn{std::move(fn)} {}

    const std::string& name() const noexcept { return m_name; }
    void invoke() const { m_fn(); }

private:
    std::string m_name;
    std::function<void()> m_fn;
};

struct auto_test_reg {
    explicit auto_test_reg(test_reg reg) noexcept { tests()[reg.name()] = std::move(reg); }

    static std::map<std::string, test_reg>& tests();
};

// NOLINTBEGIN(cppcoreguidelines-macro-usage, misc-use-anonymous-namespace)

#define MAKE_TEST_CASE_NAME2(name, counter) name##counter
#define MAKE_TEST_CASE_NAME(name, counter) MAKE_TEST_CASE_NAME2(name, counter)

#define TEST_CASE_INTERNAL(name, counter)                                                                              \
    static void MAKE_TEST_CASE_NAME(garvirt_test_driver_case_, counter)();                                             \
    namespace {                                                                                                        \
    const ::gv::test::auto_test_reg MAKE_TEST_CASE_NAME(garvirt_test_driver_case_reg_, counter){                       \
        {name, MAKE_TEST_CASE_NAME(garvirt_test_driver_case_, counter)}};                                              \
    }                                                                                                                  \
    static void MAKE_TEST_CASE_NAME(garvirt_test_driver_case_, counter)()

#define TEST_CASE(name) TEST_CASE_INTERNAL(name, __LINE__)

#define REQUIRE(...)                                                                                                   \
    if (!static_cast<bool>(__VA_ARGS__)) {                                                                             \
        throw ::gv::test::test_failure{::gv::test::failure_info{#__VA_ARGS__, __FILE__, __LINE__}};                    \
    }

#define REQUIRE_FALSE(...)                                                                                             \
    if (static_cast<bool>(__VA_ARGS__)) {                                                                              \
        throw ::gv::test::test_failure{::gv::test::failure_info{#__VA_ARGS__, __FILE__, __LINE__}};                    \
    }

#define REQUIRE_THROW(exception, ...)                                                                                  \
    {                                                                                                                  \
        bool did_throw{};                                                                                              \
        bool unexpected_exception_thrown{};                                                                            \
        try {                                                                                                          \
            (__VA_ARGS__)();                                                                                           \
        } catch (const exception&) {                                                                                   \
            did_throw = true;                                                                                          \
        } catch (...) {                                                                                                \
            did_throw = true;                                                                                          \
            unexpected_exception_thrown = true;                                                                        \
        }                                                                                                              \
        REQUIRE(did_throw);                                                                                            \
        REQUIRE(!unexpected_exception_thrown);                                                                         \
    }

#define REQUIRE_NOTHROW(...)                                                                                           \
    {                                                                                                                  \
        bool did_throw{};                                                                                              \
        try {                                                                                                          \
            (__VA_ARGS__)();                                                                                           \
        } catch (...) {                                                                                                \
            did_throw = true;                                                                                          \
        }                                                                                                              \
        REQUIRE(!did_throw);                                                                                           \
    }

#define SECTION(name)

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace gv::test
