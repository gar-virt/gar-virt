#!/usr/bin/env bash

set -e
. "$(dirname "$(realpath "${BASH_SOURCE[0]}")")/_core.sh"

static_runtime_flags='-static-libgcc -static-libstdc++'

dep_init \
    --cmake-arg -DBUILD_SHARED_LIBS=OFF \
    --cmake-arg -DBUILD_TESTING=OFF \
    --cmake-arg -DCMAKE_BUILD_TYPE=Release \
    --cmake-arg -DCMAKE_C_FLAGS="${static_runtime_flags}" \
    --cmake-arg -DCMAKE_C_STANDARD_REQUIRED=TRUE \
    --cmake-arg -DCMAKE_C_STANDARD=99 \
    --cmake-arg -DCMAKE_CXX_FLAGS="${static_runtime_flags}" \
    --cmake-arg -DCMAKE_CXX_STANDARD_REQUIRED=TRUE \
    --cmake-arg -DCMAKE_CXX_STANDARD=23 \
    --cmake-arg -DCMAKE_EXE_LINKER_FLAGS="${static_runtime_flags}" \
    --cmake-arg -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=TRUE \
    --cmake-arg -DCMAKE_MODULE_LINKER_FLAGS="${static_runtime_flags}" \
    --cmake-arg -DCMAKE_SHARED_LINKER_FLAGS="${static_runtime_flags}" \
    --cmake-arg -DOPENSSL_USE_STATIC_LIBS=ON \
    --cmake-arg -DZLIB_USE_STATIC_LIBS=ON

dep_add \
    --name zlib \
    --version 1.3.1 \
    --hash-sha256 38ef96b8dfe510d42707d9c781877914792541133e1870841463bfa73f883e32 \
    --url https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.xz \
    --subdir zlib-1.3.1

dep_add \
    --name boost \
    --version 1.91.0 \
    --hash-sha256 29c7d4f4ac36ad853b6765d03571ea60d90286775df026b4efd9f3281131972b \
    --url https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.7z \
    --subdir boost-1.91.0-1 \
    --cmake-arg -DBOOST_INCLUDE_LIBRARIES="dll;json;program_options;url;uuid" \
    --cmake-arg -DBoost_USE_STATIC_LIBS=ON

dep_add \
    --name cppcodec \
    --version 8019b8b580f8573c33c50372baec7039dfe5a8ce \
    --hash-sha256 2547d492dfc32bdd3e12674114d52f054efa0b6acbf3bbff679f94ef1cbcf844 \
    --url https://github.com/tplgy/cppcodec/archive/8019b8b580f8573c33c50372baec7039dfe5a8ce.tar.gz \
    --subdir cppcodec-8019b8b580f8573c33c50372baec7039dfe5a8ce \
    --cmake-arg -DCPPCODEC_BUILD_EXAMPLES=OFF \
    --cmake-arg -DCPPCODEC_BUILD_TESTING=OFF \
    --cmake-arg -DCPPCODEC_BUILD_TOOLS=OFF

dep_add \
    --name yaml-cpp \
    --version 0.9.0 \
    --hash-sha256 298593d9c440fd9034b8b193d96318b76d49bc97c6ceadb7b0836edf0b6d7539 \
    --url https://github.com/jbeder/yaml-cpp/releases/download/yaml-cpp-0.9.0/yaml-cpp-yaml-cpp-0.9.0.tar.gz \
    --cmake-arg -DYAML_BUILD_SHARED_LIBS=OFF \
    --cmake-arg -DYAML_CPP_BUILD_CONTRIB=OFF \
    --cmake-arg -DYAML_CPP_BUILD_TESTS=OFF \
    --cmake-arg -DYAML_CPP_BUILD_TOOLS=OFF \
    --cmake-arg -DYAML_CPP_DISABLE_UNINSTALL=ON \
    --cmake-arg -DYAML_CPP_FORMAT_SOURCE=OFF \
    --cmake-arg -DYAML_CPP_INSTALL=ON \
    --cmake-arg -DYAML_CPP_USE_STRICT_FLAGS=OFF \
    --cmake-arg -DYAML_ENABLE_PIC=OFF

dep_add \
    --name openssl \
    --version 3.6.2 \
    --hash-sha256 2beabbb4d9a472dda6788a02e14f38d101175b6081cabad8f87d279562346fb3 \
    --url https://github.com/jimmy-park/openssl-cmake/archive/refs/tags/3.6.2.tar.gz \
    --subdir openssl-cmake-3.6.2 \
    --cmake-arg -DOPENSSL_CONFIGURE_OPTIONS="no-shared;--prefix=$(get_install_prefix)" \
    --cmake-arg -DOPENSSL_INSTALL=ON \
    --cmake-arg -DOPENSSL_TEST=OFF \
    --cmake-arg -DOPENSSL_USE_CCACHE=OFF

dep_add \
    --name curl \
    --version 8.21.0 \
    --hash-sha256 aa1b66a70eace83dc624508745646c08ae561de512ab403adffb93ac87fc72e6 \
    --url https://github.com/curl/curl/releases/download/curl-8_21_0/curl-8.21.0.tar.xz \
    --subdir curl-8.21.0 \
    --cmake-arg -DBUILD_CURL_EXE=OFF \
    --cmake-arg -DBUILD_EXAMPLES=OFF \
    --cmake-arg -DBUILD_LIBCURL_DOCS=OFF \
    --cmake-arg -DBUILD_MISC_DOCS=OFF \
    --cmake-arg -DBUILD_STATIC_LIBS=ON \
    --cmake-arg -DBUILD_TESTING=OFF \
    --cmake-arg -DCURL_BROTLI=OFF \
    --cmake-arg -DCURL_DISABLE_INSTALL=OFF \
    --cmake-arg -DCURL_ENABLE_EXPORT_TARGET=ON \
    --cmake-arg -DCURL_USE_CMAKECONFIG=ON \
    --cmake-arg -DCURL_USE_LIBPSL=OFF \
    --cmake-arg -DCURL_USE_LIBSSH2=OFF \
    --cmake-arg -DCURL_USE_OPENSSL=ON \
    --cmake-arg -DCURL_USE_PKGCONFIG=OFF \
    --cmake-arg -DCURL_ZLIB=ON \
    --cmake-arg -DCURL_ZSTD=OFF \
    --cmake-arg -DENABLE_CURL_MANUAL=OFF \
    --cmake-arg -DHTTP_ONLY=ON \
    --cmake-arg -DPICKY_COMPILER=OFF \
    --cmake-arg -DUSE_LIBIDN2=OFF \
    --cmake-arg -DUSE_NGHTTP2=OFF

dep_add \
    --name abseil \
    --version 20260526.0 \
    --hash-sha256 6e1aee535473414164bf83e4ebc40240dec71a4701f8a642d906e95bea1aea0c \
    --url https://github.com/abseil/abseil-cpp/releases/download/20260526.0/abseil-cpp-20260526.0.tar.gz \
    --subdir abseil-cpp-20260526.0 \
    --cmake-arg -DABSL_ENABLE_INSTALL=ON

dep_add \
    --name protobuf \
    --version 35.1 \
    --hash-sha256 f0b6838e7522a8da96126d487068c959bc624926368f3024ac8fd03abd0a1ac4 \
    --url https://github.com/protocolbuffers/protobuf/releases/download/v35.1/protobuf-35.1.tar.gz \
    --subdir protobuf-35.1 \
    --cmake-arg -Dprotobuf_BUILD_LIBPROTOBUF=ON \
    --cmake-arg -Dprotobuf_BUILD_LIBPROTOC=ON \
    --cmake-arg -Dprotobuf_BUILD_LIBUPB=ON \
    --cmake-arg -Dprotobuf_BUILD_PROTOBUF_BINARIES=ON \
    --cmake-arg -Dprotobuf_BUILD_PROTOC_BINARIES=ON \
    --cmake-arg -Dprotobuf_BUILD_SHARED_LIBS=OFF \
    --cmake-arg -Dprotobuf_INSTALL=ON \
    --cmake-arg -Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON \
    --cmake-arg -Dprotobuf_WITH_ZLIB=OFF

dep_add \
    --name c-ares \
    --version 1.34.8 \
    --hash-sha256 c222b6d681096f9444d2c4863d2c1174019e27cacca0a4a5c114d36dd7d7bf78 \
    --url https://github.com/c-ares/c-ares/releases/download/v1.34.8/c-ares-1.34.8.tar.gz \
    --subdir c-ares-1.34.8 \
    --cmake-arg -DCARES_STATIC=ON \
    --cmake-arg -DCARES_SHARED=OFF \
    --cmake-arg -DCARES_INSTALL=ON \
    --cmake-arg -DCARES_BUILD_TOOLS=OFF

dep_add \
    --name re2 \
    --version 2025-11-05 \
    --hash-sha256 87f6029d2f6de8aa023654240a03ada90e876ce9a4676e258dd01ea4c26ffd67 \
    --url https://github.com/google/re2/releases/download/2025-11-05/re2-2025-11-05.tar.gz \
    --subdir re2-2025-11-05 \
    --cmake-arg -DRE2_INSTALL=ON

dep_add \
    --name grpc \
    --version 1.83.0 \
    --hash-sha256 90d453393a9d41215df546103b10b33b9566df79cdf6f49dc67f6c4d044d090d \
    --url https://github.com/grpc/grpc/archive/refs/tags/v1.83.0.tar.gz \
    --subdir grpc-1.83.0 \
    --cmake-arg -DgRPC_ABSL_PROVIDER=package \
    --cmake-arg -DgRPC_BENCHMARK_PROVIDER=none \
    --cmake-arg -DgRPC_BUILD_CODEGEN=ON \
    --cmake-arg -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON \
    --cmake-arg -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF \
    --cmake-arg -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
    --cmake-arg -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
    --cmake-arg -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
    --cmake-arg -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
    --cmake-arg -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF \
    --cmake-arg -DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=OFF \
    --cmake-arg -DgRPC_BUILD_TESTS=OFF \
    --cmake-arg -DgRPC_CARES_PROVIDER=package \
    --cmake-arg -DgRPC_DOWNLOAD_ARCHIVES=OFF \
    --cmake-arg -DgRPC_INSTALL=ON \
    --cmake-arg -DgRPC_PROTOBUF_PROVIDER=package \
    --cmake-arg -DgRPC_RE2_PROVIDER=package \
    --cmake-arg -DgRPC_SSL_PROVIDER=package \
    --cmake-arg -DgRPC_USE_SYSTEMD=OFF \
    --cmake-arg -DgRPC_ZLIB_PROVIDER=package

dep_commit
