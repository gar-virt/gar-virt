add_library(checks INTERFACE)

if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
    target_compile_options(checks INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
