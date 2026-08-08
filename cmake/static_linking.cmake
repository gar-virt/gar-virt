include_guard()

set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)

add_compile_options(-static-libgcc -static-libstdc++)
add_link_options(-static-libgcc -static-libstdc++)
