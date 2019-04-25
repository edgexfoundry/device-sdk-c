cmake_minimum_required (VERSION 3.0)
project (Csdk LANGUAGES C)

# Package support

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Versioning

file (STRINGS ${CMAKE_SOURCE_DIR}/../VERSION CSDK_DOT_VERSION)
string (REGEX REPLACE "....$" "" VERSION_MAJOR ${CSDK_DOT_VERSION})
string (REGEX REPLACE "..$" "" VERSION_MINOR ${CSDK_DOT_VERSION})
string (REGEX REPLACE "^.." "" VERSION_MINOR ${VERSION_MINOR})
string (REGEX REPLACE "^...." "" VERSION_PATCH ${CSDK_DOT_VERSION})

# Find required libraries

find_package (LibMicroHTTP REQUIRED)
if (NOT LIBMICROHTTP_FOUND)
  message (FATAL_ERROR "microhttp library or header not found")
endif ()
find_package (CURL REQUIRED)
if (NOT CURL_FOUND)
  message (FATAL_ERROR "curl library or header not found")
endif ()
find_package (LibYAML REQUIRED)
if (NOT LIBYAML_FOUND)
  message (FATAL_ERROR "yaml library or header not found")
endif ()
find_package (LibUUID REQUIRED)
if (NOT LIBUUID_FOUND)
  message (FATAL_ERROR "UUID library or header not found")
endif ()

message (STATUS "C SDK ${CSDK_DOT_VERSION} for ${CMAKE_SYSTEM_NAME}")

# Configuration variables

set (CSDK_BUILD_DEBUG OFF CACHE BOOL "Build Debug")
set (CSDK_BUILD_LCOV OFF CACHE BOOL "Build LCov")

# Configure for different target systems

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set (CPACK_GENERATOR TGZ)
endif ()
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set (CPACK_GENERATOR TGZ)
endif ()

# Build modules

add_subdirectory (c)
 
# Configure installer

set (CPACK_PACKAGE_NAME "Csdk")
set (CPACK_PACKAGE_VENDOR "IoTech")
set (CPACK_PACKAGE_CONTACT "support@iotechsys.com")
set (CPACK_PACKAGE_DESCRIPTION_SUMARY "EdgeXpert Device SDK for C")
set (CPACK_PACKAGE_VERSION "${CSDK_DOT_VERSION}")
set (CPACK_PACKAGE_FILE_NAME "csdk-${CPACK_PACKAGE_VERSION}")
set (CPACK_VERSION_MAJOR "${VERSION_MAJOR}")
set (CPACK_VERSION_MINOR "${VERSION_MINOR}")
set (CPACK_VERSION_PATCH "${VERSION_PATCH}")

include (CPack)
