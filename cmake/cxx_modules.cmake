if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4.0")
    # 4.4.0
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.3.0")
    # 4.3.0-4.3.4
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.3")
    # 4.0.3-4.2.7 - Same UUID as below
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.0")
    # 4.0.0-4.0.2
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.31.8")
    # 3.31.8-3.31.12
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30.0")
    # 3.30.0-3.31.7
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
endif()

function(add_std_module)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30.0")
        set(CMAKE_CXX_MODULE_STD ON PARENT_SCOPE)
        return()
    endif()

    if(NOT LINUX OR NOT ((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$")))
        message(WARNING "Unsupported platform/compiler - will not generate std modules")
        return()
    endif()

    foreach(TARGET SUB_PATH IN ZIP_LISTS "std;bits/std.cc" "std.compat;bits/std.compat.cc")
        add_library("${TARGET}" OBJECT)
        find_path(BASE_DIR "${SUB_PATH}" PATHS ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
        if(NOT BASE_DIR)
            message(WARNING "Module file \"${SUB_PATH}\" not found - will not create ${TARGET} target")
            continue()
        endif()
        set_source_files_properties("${BASE_DIR}/${SUB_PATH}" PROPERTIES LANGUAGE CXX)
        target_sources("${TARGET}" PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${BASE_DIR}" FILES "${BASE_DIR}/${SUB_PATH}")
    endforeach()
endfunction()
