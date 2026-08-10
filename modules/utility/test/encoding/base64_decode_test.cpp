#include <gv/test/test_driver.hpp>
#include <utility/encoding/base64.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace gv::utility;
using namespace std;
using namespace std::string_literals;

namespace {
template <typename T> void test_base64_decode_to_bytes() {
    const vector<T> expected_0;
    const vector<T> expected_1 = {T{0}};
    const vector<T> expected_2 = {T{0}, T{127}};
    const vector<T> expected_3 = {T{0}, T{127}, T{128}};
    const vector<T> expected_4 = {T{0}, T{127}, T{128}, T{255}};
    REQUIRE(ranges::equal(expected_0, base64_decode_return<vector<T>>("")));
    REQUIRE(ranges::equal(expected_1, base64_decode_return<vector<T>>("AA==")));
    REQUIRE(ranges::equal(expected_2, base64_decode_return<vector<T>>("AH8=")));
    REQUIRE(ranges::equal(expected_3, base64_decode_return<vector<T>>("AH+A")));
    REQUIRE(ranges::equal(expected_4, base64_decode_return<vector<T>>("AH+A/w==")));
}
} // namespace

TEST_CASE("base64_decode_return<string>") {
    REQUIRE(base64_decode_return<string>("") == "");
    REQUIRE(base64_decode_return<string>("Zg==") == "f");
    REQUIRE(base64_decode_return<string>("Zm8=") == "fo");
    REQUIRE(base64_decode_return<string>("Zm9v") == "foo");
    REQUIRE(base64_decode_return<string>("Zm9vYg==") == "foob");
    REQUIRE(base64_decode_return<string>("Zm9vYmE=") == "fooba");
    REQUIRE(base64_decode_return<string>("Zm9vYmFy") == "foobar");
}

TEST_CASE("base64_decode_return<u8string>") {
    REQUIRE(base64_decode_return<u8string>("") == u8"");
    REQUIRE(base64_decode_return<u8string>("8J+YgA==") == u8"😀");
    REQUIRE(base64_decode_return<u8string>("w6bDuMOl") == u8"æøå");
    REQUIRE(base64_decode_return<u8string>("44OG44K544OI") == u8"テスト");
}

TEST_CASE("base64_decode_return<vector<uint8_t>>") { test_base64_decode_to_bytes<uint8_t>(); }

TEST_CASE("base64_decode_return<vector<byte>>") { test_base64_decode_to_bytes<byte>(); }
