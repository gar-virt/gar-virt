find_package(Boost REQUIRED)
find_package(CURL REQUIRED)

add_library(utility)
target_sources(utility PRIVATE
    src/concurrency/thread_pool_executor.cpp
    src/encoding/base64.cpp
    src/error.cpp
    src/env.cpp
    src/datetime.cpp
    src/fs/filesystem.cpp
    src/fs/temporary_file.cpp
    src/http.cpp
    src/log/ansi.cpp
    src/log/global_logger.cpp
    src/log/stdout.cpp
    src/shutdown_signal.cpp
    src/string.cpp
    src/uuid.cpp
)
target_include_directories(utility PUBLIC include)
target_link_libraries(utility PRIVATE
    Boost::headers
    CURL::libcurl
    cppcodec
    checks
)
