/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "base64.h"

#include <string.h>
#include <inttypes.h>

#define WHITESPACE 64
#define EQUALS     65
#define INVALID    66

static const unsigned char dec[] =
{
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 64, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 62, 66, 66, 66, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
  61, 66, 66, 66, 65, 66, 66, 66,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
  11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 66, 66, 66, 66,
  66, 66, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
  43, 44, 45, 46, 47, 48, 49, 50, 51, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
  66, 66, 66, 66, 66, 66, 66, 66, 66
};

static const char enc[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t edgex_b64_encodesize (size_t binsize)
{
  size_t result = binsize / 3 * 4;    // Four chars per three bytes
  if (binsize % 3) { result += 4; }   // Another four for trailing one or two
  return result + 1;                  // Plus '\0'
}

size_t edgex_b64_maxdecodesize (const char *in)
{
  size_t inLen = strlen (in);
  return (inLen % 4) ? inLen / 4 * 3 + 2 : inLen / 4 * 3;
}

/* BASE64 encode/decode functions based on public domain code at 
 * https://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64
 */

bool edgex_b64_decode (const char *in, void *outv, size_t *outLen)
{
  uint8_t *out = (uint8_t *) outv;
  int iter = 0;
  uint32_t buf = 0;
  size_t len = 0;

  while (*in)
  {
    unsigned char c = dec[(unsigned char) (*in++)];

    if (c == WHITESPACE) continue;   // skip whitespace
    if (c == INVALID) return false;  // invalid input, return error
    if (c == EQUALS) break;          // pad character, end of data

    buf = buf << 6 | c;

    /* Every four symbols we will have filled the buffer. Split it into bytes */

    if (++iter == 4)
    {
      if ((len += 3) > *outLen) { return false; } // buffer overflow
      *(out++) = (buf >> 16) & 255;
      *(out++) = (buf >> 8) & 255;
      *(out++) = buf & 255;
      buf = 0;
      iter = 0;
    }
  }
  if (iter == 3)
  {
    if ((len += 2) > *outLen) { return false; } // buffer overflow
    *(out++) = (buf >> 10) & 255;
    *(out++) = (buf >> 2) & 255;
  }
  else if (iter == 2)
  {
    if (++len > *outLen) { return false; } // buffer overflow
    *(out++) = (buf >> 4) & 255;
  }

  *outLen = len; // modify outLen to reflect the actual output size
  return true;
}

bool edgex_b64_encode (const void *in, size_t inLen, char *out, size_t outLen)
{
  const uint8_t *data = (const uint8_t *) in;
  size_t resultIndex = 0;
  size_t x;
  uint32_t n = 0;
  uint8_t n0, n1, n2, n3;

  if (outLen < edgex_b64_encodesize (inLen))
  {
    return false;
  }

  /* iterate over the length of the string, three characters at a time */
  for (x = 0; x < inLen; x += 3)
  {
    /* combine up to three bytes into 24 bits */

    n = ((uint32_t) data[x]) << 16;
    if ((x + 1) < inLen)
    {
      n += ((uint32_t) data[x + 1]) << 8;
    }
    if ((x + 2) < inLen)
    {
      n += data[x + 2];
    }

    /* split into four 6-bit numbers */

    n0 = (uint8_t) (n >> 18) & 63;
    n1 = (uint8_t) (n >> 12) & 63;
    n2 = (uint8_t) (n >> 6) & 63;
    n3 = (uint8_t) n & 63;

    /* One byte -> two characters */

    out[resultIndex++] = enc[n0];
    out[resultIndex++] = enc[n1];

    /* Two bytes -> three characters */

    if ((x + 1) < inLen)
    {
      out[resultIndex++] = enc[n2];
    }

    /* Three bytes -> four characters */

    if ((x + 2) < inLen)
    {
      out[resultIndex++] = enc[n3];
    }
  }

  /* Pad to multiple of four characters */

  while (resultIndex % 4)
  {
    out[resultIndex++] = '=';
  }

  /* Terminate string */

  out[resultIndex] = 0;
  return true;
}
