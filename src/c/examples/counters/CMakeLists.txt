add_executable (device-counter device-counter.c)
target_include_directories (device-counter PRIVATE ../../../../include)
target_link_libraries (device-counter PRIVATE csdk)
