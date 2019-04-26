find_path (LIBUUID_INCLUDE_DIR uuid/uuid.h)
find_library (LIBUUID_LIBRARIES NAMES uuid libuuid)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LIBUUID DEFAULT_MSG LIBUUID_LIBRARIES LIBUUID_INCLUDE_DIR)
