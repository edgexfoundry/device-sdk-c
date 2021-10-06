find_path (LIBPROM_INCLUDE_DIR prom.h )
find_library (LIBPROM_LIBRARIES NAMES prom libprom)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LIBPROM DEFAULT_MSG LIBPROM_LIBRARIES LIBPROM_INCLUDE_DIR)
