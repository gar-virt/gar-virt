configure_file(src/version.cpp.in generated/version.cpp @ONLY)

generate_scm_version(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scm.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/scm.cpp"
    "PROJECT_SCM_VERSION")

add_library(project_meta)

target_include_directories(project_meta PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(project_meta PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/scm.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.cpp"
)
