add_executable (device-gyro device-gyro.c)
target_include_directories (device-gyro PRIVATE ../../../../include)
target_link_libraries (device-gyro PRIVATE csdk)
