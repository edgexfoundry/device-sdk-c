add_executable (device-random device-random.c)
target_include_directories (device-random PRIVATE ../../../../include)
target_link_libraries (device-random PRIVATE csdk)
