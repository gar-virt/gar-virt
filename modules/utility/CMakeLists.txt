if(WIN32)
    set(UUID_LIBRARIES rpcrt4)
elseif(UNIX)
    find_library(UUID_LIBRARIES uuid REQUIRED)
endif()

add_library(utility)
target_sources(utility PRIVATE
    src/encoding/base64.cpp
    src/error.cpp
    src/env.cpp
    src/datetime.cpp
    src/file_handle.cpp
    src/filesystem.cpp
    src/http.cpp
    src/log/ansi.cpp
    src/log/global_logger.cpp
    src/log/stdout.cpp
    src/shutdown_signal.cpp
    src/spawn.cpp
    src/string.cpp
    src/uuid.cpp
)
target_include_directories(utility PUBLIC include)
target_link_libraries(utility PRIVATE ${UUID_LIBRARIES} cppcodec)
