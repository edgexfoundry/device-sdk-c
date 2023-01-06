add_executable (device-gyro device-gyro.c)
target_include_directories (device-gyro PRIVATE ../../../../include ${INCLUDE_DIRS})
target_link_libraries (device-gyro PRIVATE csdk)
