# Compile the std module allowing us to "import std" without enabling experimental CMake support
add_library(std_wrapper OBJECT)

set(BASE_DIRS)
set(SUB_PATHS)

# Avoid having to keep up-to-date with CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
if(NOT CMAKE_CXX_MODULE_STD)
    if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
        target_compile_options(std_wrapper PRIVATE -fmodules)
        if(LINUX)
            find_path(STD_CC_DIR "bits/std.cc" PATHS ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
            if(STD_CC_DIR)
                list(APPEND BASE_DIRS "${STD_CC_DIR}")
                list(APPEND SUB_PATHS "bits/std.cc")
            endif()
            find_path(STD_COMPAT_CC_DIR "bits/std.compat.cc" PATHS ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
            if(STD_COMPAT_CC_DIR)
                list(APPEND BASE_DIRS "${STD_COMPAT_CC_DIR}")
                list(APPEND SUB_PATHS "bits/std.compat.cc")
            endif()
        endif()
    endif()
endif()

foreach(BASE SUB IN ZIP_LISTS BASE_DIRS SUB_PATHS)
    set(SOURCE "${BASE}/${SUB}")
    set_source_files_properties("${SOURCE}" PROPERTIES LANGUAGE CXX)
    target_sources(std_wrapper PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS "${BASE}"
        FILES "${SOURCE}")
endforeach()
