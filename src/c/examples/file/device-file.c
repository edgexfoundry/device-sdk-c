/* Simple file monitor illustrating post_readings usage */
/*
 * Copyright (c) 2020
 * IoTech Ltd
 */

#include "devsdk/devsdk.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>

#define EVENT_BUF_LEN 4096
#define VERSION "1.0"

static sig_atomic_t running = true;

typedef struct file_driver
{
  iot_logger_t *lc;
} file_driver;

static void file_signal_handler(int i)
{
  if ((i == SIGINT) || (i == SIGTERM))
  {
    running = false;
  }
}

static uint8_t *file_readfile (const char *filename, uint32_t *size)
{
  uint8_t *result = NULL;
  FILE *fd = fopen (filename, "r");
  if (fd)
  {
    fseek (fd, 0L, SEEK_END);
    *size = ftell (fd);
    rewind (fd);
    result = malloc (*size);
    if (fread (result, *size, 1, fd) != 1)
    {
      free (result);
      result = NULL;
    }
    fclose (fd);
  }
  return result;
}

static bool file_init (void *impl, struct iot_logger_t *lc, const iot_data_t *config)
{
  file_driver *driver = (file_driver *)impl;

  driver->lc = lc;
  iot_log_info (lc, "Initialising File Monitor Device Service");

  return true;
}

static bool file_get_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  devsdk_commandresult *readings,
  const devsdk_nvpairs *qparams,
  iot_data_t **exception
)
{
  return false;
}

static bool file_put_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  const devsdk_nvpairs *qparams,
  iot_data_t **exception
)
{
  return false;
}

static void file_stop (void *impl, bool force)
{
}

int main (int argc, char *argv[])
{
  int fd, wd, retval = 0;
  ssize_t i = 0, len;
  fd_set rfds;
  char buf[EVENT_BUF_LEN];
  devsdk_service_t *service;
  iot_data_t *dnames = iot_data_alloc_map (IOT_DATA_INT32);
  iot_data_t *fnames = iot_data_alloc_map (IOT_DATA_INT32);
  file_driver *impl = calloc (1, sizeof (file_driver));

  fd = inotify_init ();
  if (fd < 0)
  {
    iot_log_error (iot_logger_default (), "inotify init failure");
    return -1;
  }

  devsdk_error e;
  e.code = 0;
  running = true;

  /* Device Callbacks */
  devsdk_callbacks fileImpls =
  {
    file_init,         /* Initialize */
    NULL,              /* Reconfigure */
    NULL,              /* Discovery */
    file_get_handler,  /* Get */
    file_put_handler,  /* Put */
    file_stop          /* Stop */
  };

  /* Initalise a new device service */
  service = devsdk_service_new ("device-file", VERSION, impl, fileImpls, &argc, argv, &e);

  if (service)
  {
    int n = 1;
    while (n < argc)
    {
      if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
      {
        printf ("Options:\n");
        printf ("  -h, --help\t\t: Show this text\n");
        devsdk_usage ();
        free (impl);
        return 0;
      }
      else
      {
        printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
        free (impl);
        return 0;
      }
    }

    /* Start the device service*/
    devsdk_service_start (service, NULL, &e);
    if (e.code == 0)
    {
      signal (SIGINT, file_signal_handler);
      signal (SIGTERM, file_signal_handler);
      sleep (1);

      devsdk_devices *devs = devsdk_get_devices (service);
      if (devs == NULL)
      {
        iot_log_error (impl->lc, "No devices found, exiting");
        running = false;
      }

      /* Set up watchers, map watch ids to device and file names */

      for (devsdk_devices *d = devs; d; d = d->next)
      {
        const devsdk_nvpairs *props = devsdk_protocols_properties (d->protocols, "Filename");
        const char *fname = devsdk_nvpairs_value (props, "Name");
        iot_log_info (impl->lc, "Device %s: watching file %s", d->devname, fname);
        if ((wd = inotify_add_watch (fd, fname, IN_MODIFY)) < 0)
        {
          iot_log_error (impl->lc, "inotify add watch failure for %s", fname);
          retval = -1;
          running = false;
          break;
        }
        iot_data_map_add (fnames, iot_data_alloc_i32 (wd), iot_data_alloc_string (fname, IOT_DATA_COPY));
        iot_data_map_add (dnames, iot_data_alloc_i32 (wd), iot_data_alloc_string (d->devname, IOT_DATA_COPY));
      }
      devsdk_free_devices (devs);

      /* run until the service is interrupted */
      while (running)
      {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        if (select (fd+1, &rfds, NULL, NULL, NULL) > 0)
        {
          len = read (fd, buf, EVENT_BUF_LEN);
          if (len < 0)
          {
            iot_log_error (impl->lc, "inotify read error");
          }
          else
          {
            i = 0;
            while (i < len)
            {
              uint8_t *data;
              uint32_t size;
              struct inotify_event *event = (struct inotify_event *)&buf[i];

              /* Something happened, look up which device and what filename */
              iot_data_t *key = iot_data_alloc_i32 (event->wd);
              const char *dname = iot_data_string (iot_data_map_get (dnames, key));
              const char *fname = iot_data_string (iot_data_map_get (fnames, key));
              iot_log_info (impl->lc, "Reading updated file %s for device %s", fname, dname);

              /* Load the file contents */
              data = file_readfile (fname, &size);
              if (data)
              {
                /* Set up a commandresult. The deviceResource for our profiles is "File" */
                devsdk_commandresult results[1];
                iot_log_info (impl->lc, "File size: %" PRIu32, size);
                results[0].origin = 0;
                results[0].value = iot_data_alloc_array (data, size, IOT_DATA_UINT8, IOT_DATA_TAKE);

                /* Trigger an event */
                devsdk_post_readings (service, dname, "File", results);

                /* Cleanup the value. Note that as we used IOT_DATA_TAKE, the buffer allocated in file_readfile is free'd here */
                iot_data_free (results[0].value);
              }
              else
              {
                iot_log_error (impl->lc, "Error reading file %s", event->name);
              }
              iot_data_free (key);
              i += (sizeof (struct inotify_event) + event->len);
            }
          }
        }
      }
      devsdk_service_stop (service, true, &e);
    }
    else
    {
      iot_log_error (iot_logger_default (), "Service start failed");
    }
    devsdk_service_free (service);
  }
  else
  {
    iot_log_error (iot_logger_default (), "Service creation failed");
  }

  close (fd);
  free (impl);
  iot_data_free (fnames);
  iot_data_free (dnames);
  return retval;
}
