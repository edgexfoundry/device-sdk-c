find_path (LIBYAML_INCLUDE_DIR yaml.h)
find_library (LIBYAML_LIBRARIES NAMES yaml libyaml)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LIBYAML DEFAULT_MSG LIBYAML_LIBRARIES LIBYAML_INCLUDE_DIR)
