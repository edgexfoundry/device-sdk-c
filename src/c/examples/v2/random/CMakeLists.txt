add_executable (device-randomv2 device-random.c)
target_include_directories (device-randomv2 PRIVATE ../../../../include)
target_link_libraries (device-randomv2 PRIVATE csdk)
