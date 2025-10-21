if(WIN32)
    set(UUID_LIBRARIES rpcrt4)
elseif(UNIX)
    find_library(UUID_LIBRARIES uuid REQUIRED)
endif()

add_library(utility SHARED)
target_sources(utility PRIVATE
    src/env.cpp
    src/file_handle.cpp
    src/filesystem.cpp
    src/spawn.cpp
    src/string.cpp
    src/uuid.cpp
)
target_include_directories(utility PUBLIC include)
target_link_libraries(utility PRIVATE ${UUID_LIBRARIES})

target_sources(utility PUBLIC FILE_SET HEADERS)
