add_executable (template-discovery template.c)
target_include_directories (template-discovery PRIVATE ../../../../include)
target_link_libraries (template-discovery PRIVATE csdk)
