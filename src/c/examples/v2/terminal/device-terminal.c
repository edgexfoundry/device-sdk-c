/* Pseudo-device service allowing display of messages in a term using C SDK */

/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <unistd.h>
#include <signal.h>
#include <curses.h>
#include <stdio.h>

#include "devsdk/devsdk.h"

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }

typedef struct terminal_driver
{
  iot_logger_t * lc;
} terminal_driver;

static bool terminal_writeMsg
(
  terminal_driver * driver,
  uint32_t nvalues,
  const devsdk_commandrequest * requests,
  const iot_data_t * values[]
)
{
  int x = 0;
  int y = 0;
  const char * msg = NULL;
  for (uint32_t i = 0; i < nvalues; i++)
  {
    const char * param = devsdk_nvpairs_value (requests[i].attributes, "parameter");

    if (param)
    {
      if (strcmp (param, "x") == 0)
      {
        x = (iot_data_i32 (values[i])) % COLS;
      }
      else if (strcmp (param, "y") == 0)
      {
        y = (iot_data_i32 (values[i])) % LINES;
      }
      else if (strcmp (param, "msg") == 0)
      {
        msg = iot_data_string (values[i]);
      }
      else if (strcmp (param, "cmd") == 0)
      {
        // skip
      }
      else
      {
        iot_log_warn (driver->lc, "Unknown parameter %s supplied", param);
      }
    }
    else
    {
      iot_log_warn (driver->lc, "No parameter in device resource %s", requests[i].resname);
    }
  }

  if (msg == NULL)
  {
    iot_log_error (driver->lc, "WriteMsg request did not specify a message");
    return false;
  }

  iot_log_info (driver->lc, "Calling writeMsg (%d, %d, %s)", x, y, msg);
  erase ();
  mvaddstr (y, x, msg);
  refresh ();
  return true;
}

static bool terminal_init
  (void * impl, struct iot_logger_t * lc, const iot_data_t * config)
{
  terminal_driver * driver = (terminal_driver *) impl;
  driver->lc = lc;

  initscr ();
  cbreak ();
  noecho ();
  clear ();
  refresh ();
  return true;
}

static bool terminal_get_handler
(
  void * impl,
  const char * devname,
  const devsdk_protocols * protocols,
  uint32_t nreadings,
  const devsdk_commandrequest * requests,
  devsdk_commandresult * readings,
  const devsdk_nvpairs * q_params,
  iot_data_t ** exception
)
{
  terminal_driver * driver = (terminal_driver *) impl;
  iot_log_error (driver->lc, "GET called for terminal device. This is a write-only device");
  return false;
}

static bool terminal_put_handler
(
  void * impl,
  const char * devname,
  const devsdk_protocols * protocols,
  uint32_t nvalues,
  const devsdk_commandrequest * requests,
  const iot_data_t * values[],
  iot_data_t ** exception
)
{
  terminal_driver * driver = (terminal_driver *) impl;
  const char * command = NULL;
  for (uint32_t i = 0; i < nvalues; i++)
  {
    const char * param = devsdk_nvpairs_value (requests[i].attributes, "parameter");
    if (param && strcmp (param, "cmd") == 0)
    {
      command = iot_data_string (values[i]);
      break;
    }
  }

  if (command == NULL)
  {
    iot_log_error (driver->lc, "No command specified in PUT request");
    return false;
  }
  else if (strcmp (command, "WriteMsg") == 0)
  {
    return terminal_writeMsg (driver, nvalues, requests, values);
  }
  else
  {
    iot_log_error (driver->lc, "Unknown command %s", command);
    return false;
  }
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void terminal_stop (void * impl, bool force)
{
  endwin ();
}

int main (int argc, char * argv[])
{
  sigset_t set;
  int sigret;

  terminal_driver * impl = malloc (sizeof (terminal_driver));
  impl->lc = NULL;

  devsdk_error e;
  e.code = 0;

  devsdk_callbacks terminalImpls =
  {
    terminal_init,         /* Initialize */
    NULL,                 /* Discovery */
    terminal_get_handler,  /* Get */
    terminal_put_handler,  /* Put */
    terminal_stop          /* Stop */
  };

  devsdk_service_t * service = devsdk_service_new
    ("device-terminal", "1.0", impl, terminalImpls, &argc, argv, &e);
  ERR_CHECK (e);

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Options:\n");
      printf ("  -h, --help\t\t: Show this text\n");
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  devsdk_service_start (service, &e);
  ERR_CHECK (e);

  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigwait (&set, &sigret);

  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

  devsdk_service_free (service);
  free (impl);
  return 0;
}