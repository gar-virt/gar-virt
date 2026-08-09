configure_file(src/version.cpp.in generated/version.cpp @ONLY)

add_library(project_meta)

target_include_directories(project_meta PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(project_meta PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.cpp"
)
