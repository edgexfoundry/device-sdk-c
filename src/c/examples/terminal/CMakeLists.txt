add_executable (device-terminal device-terminal.c)
target_include_directories (device-terminal PRIVATE ../../../../include)
target_link_libraries (device-terminal PRIVATE csdk ncurses)
