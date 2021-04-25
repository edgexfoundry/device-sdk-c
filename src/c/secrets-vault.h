/*
 * Copyright (c) 2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_SECRETS_VAULT_H_
#define _EDGEX_SECRETS_VAULT_H_ 1

#include "secrets-impl.h"

void *edgex_secrets_vault_alloc (void);
extern const edgex_secret_impls edgex_secrets_vault_fns;

#endif
