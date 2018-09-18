add_library (utest_base64 STATIC base64.c)
target_include_directories (utest_base64 PRIVATE ../../../../include)
target_include_directories (utest_base64 PRIVATE ../../cunit)
target_link_libraries (utest_base64 PRIVATE csdk)
