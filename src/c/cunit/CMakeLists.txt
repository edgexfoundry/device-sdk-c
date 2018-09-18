file (GLOB C_FILES *.c)
add_library (cunit STATIC ${C_FILES})
target_include_directories (cunit PRIVATE ../../../include)
