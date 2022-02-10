include(CheckCSourceCompiles)

# Set additional C compiler flags

if (DEFINED CMAKE_C_COMPILER_ID)
  if (CMAKE_C_COMPILER_ID MATCHES GNU)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror=implicit-function-declaration")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -pipe")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    if (CSDK_BUILD_LCOV)
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
      set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
    endif ()
    if (CSDK_LEGACY_ARRAYS)
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCSDK_LEGACY_ARRAYS")
    endif ()
    if (CMAKE_BUILD_TYPE MATCHES Debug)
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
    endif ()
  endif ()
endif ()

if ("${LINUX_SYS}" STREQUAL "Alpine")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_ALPINE_")
endif ()

# Check whether we need -latomic
check_c_source_compiles("
  #include <stdatomic.h>
  #include <inttypes.h>
  int main() { _Atomic(uint64_t) a = 0; uint64_t b = 0; uint64_t c = 0; return atomic_compare_exchange_weak (&a, &b, c); }"
CSDK_HAVE_ATOMIC)

# Set default files to compile and libraries

file (GLOB C_FILES *.c iot/*.c)
set (LINK_LIBRARIES ${LIBMICROHTTP_LIBRARIES} ${CURL_LIBRARIES} ${LIBYAML_LIBRARIES} ${LIBUUID_LIBRARIES} ${LIBCBOR_LIBRARIES} ${LIBPAHO_LIBRARIES} ${LIBHIREDIS_LIBRARIES})
if (NOT CSDK_HAVE_ATOMIC)
  list (APPEND LINK_LIBRARIES atomic)
endif ()

set (IOT_HAS_XML ON)
file (STRINGS ${CMAKE_SOURCE_DIR}/c/iot/VERSION IOT_DOT_VERSION)

configure_file ("defs.h.in" "${CMAKE_SOURCE_DIR}/../include/edgex/csdk-defs.h")
configure_file ("iot/defs.h.in" "${CMAKE_SOURCE_DIR}/../include/iot/defs.h")

# Main sdk library

add_library (csdk SHARED ${C_FILES} )
target_include_directories (csdk PRIVATE ../../include)
target_link_libraries (csdk PUBLIC m ${LINK_LIBRARIES})
set_target_properties (csdk PROPERTIES DEFINE_SYMBOL "CSDK_EXPORTS")

# Build modules

add_subdirectory (examples)
 
# Configure installer

install (TARGETS csdk LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install (DIRECTORY "${CMAKE_SOURCE_DIR}/../include/" DESTINATION include )
install (FILES "${CMAKE_SOURCE_DIR}/../LICENSE" DESTINATION share/device-sdk-c)
install (FILES "${CMAKE_SOURCE_DIR}/../Attribution.txt" DESTINATION share/device-sdk-c)
