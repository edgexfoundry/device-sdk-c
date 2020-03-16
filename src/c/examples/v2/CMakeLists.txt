add_subdirectory (counters)
add_executable (template2 template.c)
target_include_directories (template2 PRIVATE ../../../../include)
target_link_libraries (template2 PRIVATE csdk)
