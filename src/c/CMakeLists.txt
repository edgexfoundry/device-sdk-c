include(CheckCSourceCompiles)

# Set additional C compiler flags

if (DEFINED CMAKE_C_COMPILER_ID)
  if (CMAKE_C_COMPILER_ID MATCHES GNU)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror=implicit-function-declaration")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -pipe")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    if (CSDK_BUILD_DEBUG)
      if (CSDK_BUILD_LCOV)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
      endif ()
    endif ()
  endif ()
endif ()

if ("${LINUX_SYS}" STREQUAL "Alpine")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__LIBMUSL__")
endif ()

# Set default files to compile and libraries

# Check whether we need -latomic
check_c_source_compiles("
  #include <stdatomic.h>
  #include <inttypes.h>
  int main() { _Atomic(uint64_t) a = 0; uint64_t b = 0; uint64_t c = 0; return atomic_compare_exchange_weak (&a, &b, c); }"
CSDK_HAVE_ATOMIC)

file (GLOB C_FILES *.c iot/*.c)
set (LINK_LIBRARIES ${LIBMICROHTTP_LIBRARIES} ${CURL_LIBRARIES} ${LIBYAML_LIBRARIES} ${LIBUUID_LIBRARIES} ${LIBCBOR_LIBRARIES})
if (NOT CSDK_HAVE_ATOMIC)
  list (APPEND LINK_LIBRARIES atomic)
endif ()

configure_file ("defs.h.in" "${CMAKE_SOURCE_DIR}/../include/edgex/csdk-defs.h")

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
