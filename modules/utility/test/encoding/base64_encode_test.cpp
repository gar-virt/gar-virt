#include <gv/test/test_driver.hpp>
#include <utility/encoding/base64.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace gv::utility;
using namespace std;
using namespace std::string_literals;

TEST_CASE("base64_encode_return<string>(s)") {
    REQUIRE(base64_encode_return<string>("") == "");
    REQUIRE(base64_encode_return<string>("f") == "Zg==");
    REQUIRE(base64_encode_return<string>("fo") == "Zm8=");
    REQUIRE(base64_encode_return<string>("foo") == "Zm9v");
    REQUIRE(base64_encode_return<string>("foob") == "Zm9vYg==");
    REQUIRE(base64_encode_return<string>("fooba") == "Zm9vYmE=");
    REQUIRE(base64_encode_return<string>("foobar") == "Zm9vYmFy");
}

TEST_CASE("base64_encode_return<string>(u8s)") {
    REQUIRE(base64_encode_return<string>(u8"") == "");
    REQUIRE(base64_encode_return<string>(u8"😀") == "8J+YgA==");
    REQUIRE(base64_encode_return<string>(u8"æøå") == "w6bDuMOl");
    REQUIRE(base64_encode_return<string>(u8"テスト") == "44OG44K544OI");
}

TEST_CASE("base64_encode_return<string>(span<uint8_t>)") {
    vector<uint8_t> v0;
    vector<uint8_t> v1 = {0};
    vector<uint8_t> v2 = {0, 127};
    vector<uint8_t> v3 = {0, 127, 128};
    vector<uint8_t> v4 = {0, 127, 128, 255};
    REQUIRE(base64_encode_return<string>(span<const uint8_t>{v0}) == "");
    REQUIRE(base64_encode_return<string>(span<const uint8_t>{v1}) == "AA==");
    REQUIRE(base64_encode_return<string>(span<const uint8_t>{v2}) == "AH8=");
    REQUIRE(base64_encode_return<string>(span<const uint8_t>{v3}) == "AH+A");
    REQUIRE(base64_encode_return<string>(span<const uint8_t>{v4}) == "AH+A/w==");
}

TEST_CASE("base64_encode_return<string>(span<byte>)") {
    vector<byte> v0;
    vector<byte> v1 = {byte{0}};
    vector<byte> v2 = {byte{0}, byte{127}};
    vector<byte> v3 = {byte{0}, byte{127}, byte{128}};
    vector<byte> v4 = {byte{0}, byte{127}, byte{128}, byte{255}};
    REQUIRE(base64_encode_return<string>(span<const byte>{v0}) == "");
    REQUIRE(base64_encode_return<string>(span<const byte>{v1}) == "AA==");
    REQUIRE(base64_encode_return<string>(span<const byte>{v2}) == "AH8=");
    REQUIRE(base64_encode_return<string>(span<const byte>{v3}) == "AH+A");
    REQUIRE(base64_encode_return<string>(span<const byte>{v4}) == "AH+A/w==");
}
