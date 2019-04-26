/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "correlation.h"

#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

static _Thread_local char *localid = NULL;

const char *edgex_device_get_crlid ()
{
  return localid;
}

void edgex_device_alloc_crlid (const char *id)
{
  edgex_device_free_crlid ();
  if (id)
  {
    localid = strdup (id);
  }
  else
  {
    uuid_t uid;
    uuid_generate (uid);
    localid = malloc (37);
    uuid_unparse (uid, localid);
  }
}

void edgex_device_free_crlid ()
{
  free (localid);
  localid = NULL;
}
