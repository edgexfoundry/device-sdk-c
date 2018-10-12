add_executable (template template.c)
target_include_directories (template PRIVATE ../../../include ${THRIFT_DIR}/include)
target_link_libraries (template PRIVATE csdk)