cmake_minimum_required (VERSION 3.0)
project (Csdk LANGUAGES C)
set (CMAKE_C_STANDARD 11)
set (CMAKE_C_EXTENSIONS Off)

# Package support

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Versioning

set (CSDK_DOT_VERSION "0.0.0")
string (REGEX REPLACE "....$" "" VERSION_MAJOR ${CSDK_DOT_VERSION})
string (REGEX REPLACE "..$" "" VERSION_MINOR ${CSDK_DOT_VERSION})
string (REGEX REPLACE "^.." "" VERSION_MINOR ${VERSION_MINOR})
string (REGEX REPLACE "^...." "" VERSION_PATCH ${CSDK_DOT_VERSION})

# Find required libraries

set (IOT_VER 1.5)

find_package (LIBMICROHTTP REQUIRED)
if (NOT LIBMICROHTTP_FOUND)
  message (FATAL_ERROR "microhttp library or header not found")
endif ()
find_package (CURL REQUIRED)
if (NOT CURL_FOUND)
  message (FATAL_ERROR "curl library or header not found")
endif ()
find_package (LIBYAML REQUIRED)
if (NOT LIBYAML_FOUND)
  message (FATAL_ERROR "yaml library or header not found")
endif ()
find_package (LIBUUID REQUIRED)
if (NOT LIBUUID_FOUND)
  message (FATAL_ERROR "UUID library or header not found")
endif ()
find_package (LIBCBOR REQUIRED)
if (NOT LIBCBOR_FOUND)
  message (FATAL_ERROR "CBOR library or header not found")
endif ()
find_package (LIBPAHO REQUIRED)
if (NOT LIBPAHO_FOUND)
  message (FATAL_ERROR "Paho MQTT library or header not found")
endif ()
find_package (IOT REQUIRED)
set (LINK_LIBRARIES ${LINK_LIBRARIES} ${IOT_LIBRARIES})
set (INCLUDE_DIRS ${INCLUDE_DIRS} ${IOT_INCLUDE_DIRS})

message (STATUS "C SDK ${CSDK_DOT_VERSION} for ${CMAKE_SYSTEM_NAME}")

# Configuration variables

set (CSDK_BUILD_LCOV OFF CACHE BOOL "Build LCov")

# Configure for different target systems

set (OS_ARCH x86_64)
set (CPACK_GENERATOR TGZ)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set (CPACK_GENERATOR ZIP)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set (CPACK_GENERATOR TGZ)
else ()
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    execute_process (COMMAND uname -m OUTPUT_VARIABLE UNAME_M OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (EXISTS /etc/os-release)
      file (STRINGS /etc/os-release RELEASE_ID REGEX "ID=")
      file (STRINGS /etc/os-release RELEASE_VERSION REGEX "VERSION_ID=")
      string (REGEX REPLACE "VERSION_ID=" "" RELEASE_VERSION ${RELEASE_VERSION})
      string (REGEX REPLACE "\"" "" RELEASE_VERSION ${RELEASE_VERSION})
    endif ()
    if (${UNAME_M} STREQUAL "aarch64")
      set (OS_ARCH arm64)
    elseif (${UNAME_M} STREQUAL "armv7l")
      set (OS_ARCH armhf)
    endif ()
    if (EXISTS /etc/alpine-release)
      if (${OS_ARCH} STREQUAL "arm64")
        set (OS_ARCH aarch64)
      elseif (${OS_ARCH} STREQUAL "armhf")
        set (OS_ARCH armv7)
      endif ()
      set (LINUX_SYS "Alpine")
    elseif (EXISTS /etc/photon-release)
      if (${OS_ARCH} STREQUAL "arm64")
        set (OS_ARCH aarch64)
      endif ()
      set (LINUX_SYS "Photon")
      set (CPACK_GENERATOR TGZ RPM)
    elseif (EXISTS /etc/centos-release)
      if (${OS_ARCH} STREQUAL "arm64")
        set (OS_ARCH aarch64)
      endif ()
      set (LINUX_SYS "CentOS")
      set (CPACK_GENERATOR TGZ RPM)
    elseif (EXISTS /etc/fedora-release)
      if (${OS_ARCH} STREQUAL "arm64")
        set (OS_ARCH aarch64)
      endif ()
      set (LINUX_SYS "Fedora")
      set (CPACK_GENERATOR TGZ RPM)
    elseif (EXISTS /etc/SuSE-release)
      set (LINUX_SYS "SuSE")
      set (CPACK_GENERATOR TGZ RPM)
    elseif (EXISTS /etc/debian_version)
      if ("${RELEASE_ID}" MATCHES "ubuntu")
        set (LINUX_SYS "Ubuntu")
      else ()
        set (LINUX_SYS "Debian")
      endif ()
      set (CPACK_GENERATOR TGZ DEB)
    elseif ("${RELEASE_ID}" MATCHES "opensuse")
      if (${OS_ARCH} STREQUAL "arm64")
        set (OS_ARCH aarch64)
      elseif (${OS_ARCH} STREQUAL "armhf")
        set (OS_ARCH armv7hl)
      endif ()
      set (LINUX_SYS "SuSE")
      set (CPACK_GENERATOR TGZ RPM)
    endif ()
  endif ()
endif ()

# Build modules

add_subdirectory (c)
 
# Configure installer

set (CPACK_PACKAGE_NAME "Csdk")
set (CPACK_PACKAGE_VENDOR "IoTech")
set (CPACK_PACKAGE_CONTACT "support@iotechsys.com")
set (CPACK_PACKAGE_DESCRIPTION_SUMARY "EdgeX Device SDK for C")
set (CPACK_PACKAGE_VERSION "${CSDK_DOT_VERSION}")
set (CPACK_PACKAGE_FILE_NAME "csdk-${CPACK_PACKAGE_VERSION}")
set (CPACK_VERSION_MAJOR "${VERSION_MAJOR}")
set (CPACK_VERSION_MINOR "${VERSION_MINOR}")
set (CPACK_VERSION_PATCH "${VERSION_PATCH}")

include (CPack)
