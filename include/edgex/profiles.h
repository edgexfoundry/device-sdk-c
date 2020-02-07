/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_PROFILES_H
#define _EDGEX_PROFILES_H 1

/**
 * @file
 * @brief This file defines the functions for profile management provided by the SDK.
 */

#include "devsdk/devsdk.h"
#include "edgex/edgex.h"

/**
 * @brief Retrieve the device profiles currently known in the SDK.
 * @param svc The device service.
 * @returns A list of device profiles.
 */

edgex_deviceprofile *edgex_profiles (devsdk_service_t *svc);

/**
 * @brief Retrieve device profile.
 * @param svc The device service.
 * @param name The device profile name.
 * @returns The requested metadata or null if the profile was not found.
 */

edgex_deviceprofile *edgex_get_deviceprofile_byname (devsdk_service_t *svc, const char *name);

/**
 * @brief Free a device profile or list of device profiles.
 * @param p The device profile or the first device profile in the list.
 */

void edgex_free_deviceprofile (edgex_deviceprofile *p);

/**
 * @brief Install a device profile
 * @param svc The device service.
 * @param fname The name of a yaml file containing the profile definition.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_add_profile (devsdk_service_t *svc, const char *fname, devsdk_error *err);

#endif
