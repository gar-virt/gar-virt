# Adds build type/configuration for sanitizers.
if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES "ASan" "TSan")
endif()

if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
    # Address + undefined behavior sanitizer
    set(CMAKE_C_FLAGS_ASAN "-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(CMAKE_CXX_FLAGS_ASAN "-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined")
    set(CMAKE_SHARED_LINKER_FLAGS_ASAN "-fsanitize=address,undefined")
    # Thread sanitizer - cannot be combined with -fsanitize=address
    set(CMAKE_C_FLAGS_TSAN "-g -O1 -fsanitize=thread")
    set(CMAKE_CXX_FLAGS_TSAN "-g -O1 -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS_TSAN "-fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS_TSAN "-fsanitize=thread")
endif()
