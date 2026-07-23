if(WIN32)
    set(UUID_LIBRARIES rpcrt4)
elseif(UNIX)
    find_library(UUID_LIBRARIES uuid REQUIRED)
endif()

add_library(utility)
target_sources(utility PUBLIC FILE_SET modules TYPE CXX_MODULES FILES
    src/algorithm.cpp
    src/concepts.cpp
    src/datetime.cpp
    src/encoding/base64.cpp
    src/env.cpp
    src/error.cpp
    src/filesystem.cpp
    src/http.cpp
    src/lifetime.cpp
    src/log/ansi.cpp
    src/log/global_logger.cpp
    src/log/level.cpp
    src/log/log.cpp
    src/log/stdout.cpp
    src/memstream.cpp
    src/shutdown_signal.cpp
    src/stream.cpp
    src/string.cpp
    src/temporary_file.cpp
    src/thread_pool_executor.cpp
    src/utility_module.cpp
    src/uuid.cpp
)
target_include_directories(utility PUBLIC include)
target_link_libraries(utility PRIVATE
    ${UUID_LIBRARIES}
    cppcodec
    checks
)
