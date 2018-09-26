/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_ERRORLIST_H_
#define _EDGEX_ERRORLIST_H_ 1

#define EDGEX_OK (edgex_error){ .code = 0, .reason = "Success" }
#define EDGEX_NO_CONF_FILE (edgex_error){ .code = 1, .reason = "Unable to open configuration file" }
#define EDGEX_CONF_PARSE_ERROR (edgex_error){ .code = 2, .reason = "Error while parsing configuration file" }
#define EDGEX_NO_DEVICE_IMPL (edgex_error){ .code = 3, .reason = "Device implementation data was null" }
#define EDGEX_BAD_CONFIG (edgex_error){ .code = 4, .reason = "Configuration is invalid" }
#define EDGEX_HTTP_SERVER_FAIL (edgex_error){ .code = 5, .reason = "Failed to start HTTP server" }
#define EDGEX_NO_DEVICE_NAME (edgex_error){ .code = 6, .reason = "No Device name was specified" }
#define EDGEX_NO_DEVICE_VERSION (edgex_error){ .code = 7, .reason = "No Device version was specified" }
#define EDGEX_NO_CTX (edgex_error){ .code = 8, .reason = "No connection context supplied" }
#define EDGEX_INVALID_ARG (edgex_error){ .code = 9, .reason = "Invalid argument" }
#define EDGEX_HTTP_GET_ERROR (edgex_error){ .code = 10, .reason = "HTTP GET failed" }
#define EDGEX_HTTP_POST_ERROR (edgex_error){ .code = 11, .reason = "HTTP POST failed" }
#define EDGEX_HTTP_POSTFILE_ERROR (edgex_error){ .code = 12, .reason = "HTTP POST failed" }
#define EDGEX_HTTP_PUT_ERROR (edgex_error){ .code = 13, .reason = "HTTP PUT failed" }
#define EDGEX_DRIVER_UNSTART (edgex_error){ .code = 14, .reason = "Protocol driver initialization failed" }
#define EDGEX_REMOTE_SERVER_DOWN (edgex_error){ .code = 15, .reason = "Remote server unresponsive" }
#define EDGEX_PROFILE_PARSE_ERROR (edgex_error){ .code = 16, .reason = "Error while parsing device profile" }
#endif
