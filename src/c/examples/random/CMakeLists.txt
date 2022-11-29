add_executable (device-random device-random.c)
target_include_directories (device-random PRIVATE ../../../../include ${INCLUDE_DIRS})
target_link_libraries (device-random PRIVATE csdk)
