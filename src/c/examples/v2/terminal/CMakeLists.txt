add_executable (device-terminalv2 device-terminal.c)
target_include_directories (device-terminalv2 PRIVATE ../../../../../include)
target_link_libraries (device-terminalv2 PRIVATE csdk ncurses)