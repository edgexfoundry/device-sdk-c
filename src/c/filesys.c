/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "filesys.h"

#include <pthread.h>
#include <string.h>
#include <dirent.h>

static pthread_mutex_t devsdk_file_mtx = PTHREAD_MUTEX_INITIALIZER;
static const char *devsdk_file_ext = NULL;

static int devsdk_file_checkext (const struct dirent *d)
{
  char *dot = strrchr (d->d_name, '.');
  if (dot)
  {
    return strcasecmp (dot + 1, devsdk_file_ext) == 0 ? 1 : 0;
  }
  else
  {
    return 0;
  }
}

devsdk_strings * devsdk_scandir (iot_logger_t *lc, const char *dir, const char *ext)
{
  devsdk_strings *result = NULL;
  devsdk_strings *entry;
  struct dirent **filenames = NULL;
  int n;
  char *fname;

  pthread_mutex_lock (&devsdk_file_mtx);
  devsdk_file_ext = ext;
  n = scandir (dir, &filenames, devsdk_file_checkext, NULL);
  pthread_mutex_unlock (&devsdk_file_mtx);

  if (n < 0)
  {
    if (errno == ENOENT || errno == ENOTDIR)
    {
       iot_log_error (lc, "No directory found at %s", dir);
    }
    else
    {
      iot_log_error (lc, "Error scanning directory %s: %s", dir, strerror (errno));
    }
  }
  else
  {
    while (n--)
    {
      fname = filenames[n]->d_name;
      entry = malloc (sizeof (devsdk_strings));
      entry->str = malloc (strlen (dir) + strlen (fname) + 2);
      strcpy (entry->str, dir);
      strcat (entry->str, "/");
      strcat (entry->str, fname);
      entry->next = result;
      result = entry;
      free (filenames[n]);
    }
    free (filenames);
  }
  return result;
}
