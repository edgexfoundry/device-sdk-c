add_executable (device-bitfields device-bitfields.c)
target_include_directories (device-bitfields PRIVATE ../../../../include)
target_link_libraries (device-bitfields PRIVATE csdk)
