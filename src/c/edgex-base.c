/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/edgex-base.h"

const char *edgex_nvpairs_value (const edgex_nvpairs *nvp, const char *name)
{
  if (name)
  {
    for (; nvp; nvp = nvp->next)
    {
      if (strcmp (nvp->name, name) == 0)
      {
        return nvp->value;
      }
    }
  }
  return NULL;
}
