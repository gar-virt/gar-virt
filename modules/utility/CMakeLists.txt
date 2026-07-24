if(WIN32)
    set(UUID_LIBRARIES rpcrt4)
elseif(UNIX)
    find_library(UUID_LIBRARIES uuid REQUIRED)
endif()

add_library(utility)
target_sources(utility PUBLIC FILE_SET CXX_MODULES FILES
    src/concurrency/concurrency_module.cpp
    src/concurrency/thread_pool_executor.cpp
    src/datetime/datetime_module.cpp
    src/encoding/base64.cpp
    src/encoding/encoding_module.cpp
    src/fs/filesystem.cpp
    src/fs/fs_module.cpp
    src/fs/temporary_file.cpp
    src/http/http_module.cpp
    src/io/io_module.cpp
    src/io/memstream.cpp
    src/io/stream.cpp
    src/log/ansi.cpp
    src/log/global_logger.cpp
    src/log/level.cpp
    src/log/log_module.cpp
    src/log/log.cpp
    src/log/stdout.cpp
    src/misc/algorithm.cpp
    src/misc/concepts.cpp
    src/misc/env.cpp
    src/misc/error.cpp
    src/misc/lifetime.cpp
    src/misc/misc_module.cpp
    src/misc/shutdown_signal.cpp
    src/misc/string.cpp
    src/misc/uuid.cpp
)
target_link_libraries(utility PRIVATE
    ${UUID_LIBRARIES}
    cppcodec
    checks
    std_wrapper
)
