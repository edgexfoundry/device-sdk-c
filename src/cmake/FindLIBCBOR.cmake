find_path (LIBCBOR_INCLUDE_DIR cbor.h)
find_library (LIBCBOR_LIBRARIES NAMES cbor libcbor)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LIBCBOR DEFAULT_MSG LIBCBOR_LIBRARIES LIBCBOR_INCLUDE_DIR)
