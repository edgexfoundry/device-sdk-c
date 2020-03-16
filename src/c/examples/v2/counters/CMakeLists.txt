add_executable (device-counterv2 device-counter.c)
target_include_directories (device-counterv2 PRIVATE ../../../../include)
target_link_libraries (device-counterv2 PRIVATE csdk)
