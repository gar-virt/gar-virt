add_library(checks INTERFACE)

if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
    target_compile_options(checks INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-missing-field-initializers
    )
endif()

function(suppress_cpp_warnings TARGET)
    # Suppress warnings from abseil including <ciso646>:
    # "<ciso646> is not a standard header since C++20, use <version> to detect implementation-specific macros"
    if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
        target_compile_options("${TARGET}" PRIVATE "-Wno-cpp")
    endif()
endfunction()
