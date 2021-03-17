find_path (LIBPAHO_INCLUDE_DIR MQTTClient.h)
find_library (LIBPAHO_LIBRARIES NAMES paho-mqtt3c libpaho-mqtt3c)
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LIBPAHO DEFAULT_MSG LIBPAHO_LIBRARIES LIBPAHO_INCLUDE_DIR)
