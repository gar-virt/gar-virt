include_guard()
include("${CMAKE_CURRENT_LIST_DIR}/options.cmake")

if(GARVIRT_ENABLE_STATIC_LINKING)
    set(OPENSSL_USE_STATIC_LIBS ON)
    set(ZLIB_USE_STATIC_LIBS ON)

    add_compile_options(-static-libgcc -static-libstdc++)
    add_link_options(-static-libgcc -static-libstdc++)
endif()
