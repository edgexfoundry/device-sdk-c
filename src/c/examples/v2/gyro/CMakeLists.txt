add_executable (device-gyrov2 device-gyro.c)
target_include_directories (device-gyrov2 PRIVATE ../../../../../include)
target_link_libraries (device-gyrov2 PRIVATE csdk)