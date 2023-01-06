add_executable (device-file device-file.c)
target_include_directories (device-file PRIVATE ../../../../include ${INCLUDE_DIRS})
target_link_libraries (device-file PRIVATE csdk)
