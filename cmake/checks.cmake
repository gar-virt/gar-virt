include("${CMAKE_CURRENT_LIST_DIR}/options.cmake")

function(suppress_cpp_warnings TARGET)
    # Suppress warnings from abseil including <ciso646>:
    # "<ciso646> is not a standard header since C++20, use <version> to detect implementation-specific macros"
    if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
        target_compile_options("${TARGET}" PRIVATE "-Wno-cpp")
    endif()
endfunction()

# Helps analysis tools compile source with the correct compiler flags
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(checks INTERFACE)

if(GARVIRT_ENABLE_CXX_WARN)
    # Enable compiler warnings
    if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
        target_compile_options(checks INTERFACE
            -Werror
            -Wall
            -Wextra
            -Wpedantic
            -Wno-missing-field-initializers
        )
    endif()
endif()

if(GARVIRT_ENABLE_CLANG_TIDY)
    # Enable clang-tidy
    if((CMAKE_C_COMPILER_ID MATCHES "Clang$") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang$"))
        find_program(CLANG_TIDY_EXE clang-tidy REQUIRED)
        if(CLANG_TIDY_EXE)
            set_target_properties(checks PROPERTIES
                C_CLANG_TIDY "${CLANG_TIDY_EXE}"
                CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        endif()
    endif()
endif()
