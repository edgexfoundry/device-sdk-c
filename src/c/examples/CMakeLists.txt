add_executable (simple simple.c)
target_include_directories (simple PRIVATE ../../../include ${THRIFT_DIR}/include)
target_link_libraries (simple PRIVATE csdk)
