add_executable(garvirt_utility_tests)
target_sources(garvirt_utility_tests PRIVATE
    encoding/base64_decode_test.cpp
    encoding/base64_encode_test.cpp
)
target_include_directories(garvirt_utility_tests PRIVATE src)
target_link_libraries(garvirt_utility_tests PRIVATE garvirt_test_driver utility)
garvirt_discover_tests(garvirt_utility_tests)
