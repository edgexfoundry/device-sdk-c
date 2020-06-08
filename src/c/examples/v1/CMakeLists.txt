add_executable (templatev1 template.c)
target_include_directories (templatev1 PRIVATE ../../../../include)
target_link_libraries (templatev1 PRIVATE csdk)
