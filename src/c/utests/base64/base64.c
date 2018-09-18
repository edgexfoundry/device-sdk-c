/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "CUnit.h"
#include "base64.h"
#include "../src/c/base64.h"

static int suite_init (void)
{
  return 0;
}

static int suite_clean (void)
{
  return 0;
}

static void test_rtrip1 (void)
{
  const char *input = "£$%^&*()_+[]{}#~";
  char encoded[25];
  char decoded[16];
  size_t outlen;

  for (size_t size = 1; size <= 16; size++)
  {
    memset (encoded, 0, 25);
    memset (decoded, 0, 16);
    CU_ASSERT (edgex_b64_encode (input, size, encoded, 25));
    outlen = size;
    CU_ASSERT (edgex_b64_decode (encoded, decoded, &outlen));
    CU_ASSERT (size == outlen);
    CU_ASSERT (strncmp (input, decoded, size) == 0);
  }
}

void cunit_base64_test_init (void)
{
  CU_pSuite suite = CU_add_suite ("base64", suite_init, suite_clean);
  CU_add_test (suite, "test_rtrip1", test_rtrip1);
}
