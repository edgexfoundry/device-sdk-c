/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "autoevent.h"
#include "edgex/edgex.h"
#include "edgex/edgex_logging.h"

struct sfxstruct
{
  const char *str;
  uint64_t factor;
};

static struct sfxstruct suffixes[] =
  { { "ms", 1 }, { "s", 1000 }, { "m", 60000 }, { "h", 3600000 }, { NULL, 0 } };

static uint64_t parseTime (const char *spec)
{
  char *fend;
  uint64_t fnum = strtoul (spec, &fend, 10);
  for (int i = 0; suffixes[i].str; i++)
  {
    if (strcmp (fend, suffixes[i].str) == 0)
    {
      return fnum * suffixes[i].factor;
    }
  }
  return 0;
}
