/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_ERRORLIST_H_
#define _EDGEX_ERRORLIST_H_ 1

#define EDGEX_OK (devsdk_error){ .code = 0, .reason = "Success" }
#define EDGEX_NO_CONF_FILE (devsdk_error){ .code = 1, .reason = "Unable to open configuration file" }
#define EDGEX_CONF_PARSE_ERROR (devsdk_error){ .code = 2, .reason = "Error while parsing configuration file" }
#define EDGEX_NO_DEVICE_IMPL (devsdk_error){ .code = 3, .reason = "Device implementation data was null" }
#define EDGEX_BAD_CONFIG (devsdk_error){ .code = 4, .reason = "Configuration is invalid" }
#define EDGEX_HTTP_SERVER_FAIL (devsdk_error){ .code = 5, .reason = "Failed to start HTTP server" }
#define EDGEX_NO_DEVICE_NAME (devsdk_error){ .code = 6, .reason = "No Device name was specified" }
#define EDGEX_NO_DEVICE_VERSION (devsdk_error){ .code = 7, .reason = "No Device version was specified" }
#define EDGEX_NO_CTX (devsdk_error){ .code = 8, .reason = "No connection context supplied" }
#define EDGEX_INVALID_ARG (devsdk_error){ .code = 9, .reason = "Invalid argument" }
// errors 10..13 superceded by EDGEX_HTTP_ERROR
#define EDGEX_DRIVER_UNSTART (devsdk_error){ .code = 14, .reason = "Protocol driver initialization failed" }
#define EDGEX_REMOTE_SERVER_DOWN (devsdk_error){ .code = 15, .reason = "Remote server unresponsive" }
#define EDGEX_PROFILE_PARSE_ERROR (devsdk_error){ .code = 16, .reason = "Error while parsing device profile" }
#define EDGEX_HTTP_CONFLICT (devsdk_error){ .code = 17, .reason = "HTTP 409 Conflict" }
#define EDGEX_PROFILES_DIRECTORY (devsdk_error){ .code = 19, .reason = "Problem scanning profiles directory" }
#define EDGEX_ASSERT_FAIL (devsdk_error){ .code = 20, .reason = "A reading did not match a specified assertion string" }
#define EDGEX_HTTP_ERROR (devsdk_error){ .code = 21, .reason = "HTTP request failed" }
#endif
