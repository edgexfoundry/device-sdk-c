/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "devutil.h"

/* Macro for generating single-linked-list comparison functions.
 * Assumes a "next" pointer and that the key (name) field is a string.
 */

#define LIST_EQUAL_FUNCTION(TYPENAME,NAMEFIELD,CMPFUNC)           \
bool TYPENAME ## _equal (const TYPENAME *l1, const TYPENAME *l2)  \
{                                                                 \
  const TYPENAME *l;                                              \
  const TYPENAME *found;                                          \
  unsigned n1 = 0;                                                \
  unsigned n2 = 0;                                                \
  for (l = l1; l; l = l->next, n1++);                             \
  for (l = l2; l; l = l->next, n2++);                             \
  if (n1 != n2) return false;                                     \
  for (l = l1; l; l = l->next)                                    \
  {                                                               \
    for (found = l2; found; found = found->next)                  \
    {                                                             \
      if (strcmp (l->NAMEFIELD, found->NAMEFIELD) == 0) break;    \
    }                                                             \
    if (!found || !CMPFUNC (l, found)) return false;              \
  }                                                               \
  return true;                                                    \
}

static bool pair_equal (const edgex_nvpairs *p1, const edgex_nvpairs *p2)
{
  return (strcmp (p1->value, p2->value) == 0);
}

static LIST_EQUAL_FUNCTION(edgex_nvpairs, name, pair_equal)

static bool protocol_equal
  (const edgex_protocols *p1, const edgex_protocols *p2)
{
  return edgex_nvpairs_equal (p1->properties, p2->properties);
}

LIST_EQUAL_FUNCTION(edgex_protocols, name, protocol_equal)

static bool autoevent_equal
  (const edgex_device_autoevents *e1, const edgex_device_autoevents *e2)
{
  return
    strcmp (e1->frequency, e2->frequency) == 0 && e1->onChange == e2->onChange;
}

LIST_EQUAL_FUNCTION(edgex_device_autoevents, resource, autoevent_equal)
