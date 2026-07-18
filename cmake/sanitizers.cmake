# Adds build type/configuration for sanitizers.
if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES "ASan" "TSan")
endif()

block()
    if((CMAKE_C_COMPILER_ID MATCHES "((^GNU)|Clang)$") OR (CMAKE_CXX_COMPILER_ID MATCHES "((^GNU)|Clang)$"))
        # Address + undefined behavior sanitizer
        set(COMPILER_FLAGS_ASAN "-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set(LINKER_FLAGS_ASAN "-fsanitize=address,undefined")

        set(CMAKE_C_FLAGS_ASAN "${COMPILER_FLAGS_ASAN}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS_ASAN "${COMPILER_FLAGS_ASAN}" PARENT_SCOPE)
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${LINKER_FLAGS_ASAN}" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS_ASAN "${LINKER_FLAGS_ASAN}" PARENT_SCOPE)

        # Thread sanitizer - cannot be combined with -fsanitize=address
        set(COMPILER_FLAGS_TSAN "-g -O1 -fsanitize=thread")
        set(LINKER_FLAGS_TSAN "-fsanitize=thread")

        set(CMAKE_C_FLAGS_TSAN "${COMPILER_FLAGS_TSAN}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS_TSAN "${COMPILER_FLAGS_TSAN}" PARENT_SCOPE)
        set(CMAKE_EXE_LINKER_FLAGS_TSAN "${LINKER_FLAGS_TSAN}" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS_TSAN "${LINKER_FLAGS_TSAN}" PARENT_SCOPE)
    endif()
endblock()
