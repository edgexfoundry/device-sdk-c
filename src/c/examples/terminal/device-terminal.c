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
#define ERR_BUFSZ 1024
#define ERR_TERMINAL_READ "GET called for terminal device. This is a write-only device."
#define ERR_TERMINAL_NO_CMD "No command specified in PUT request."
#define ERR_TERMINAL_MSG "WriteMsg request did not specify a message."

typedef enum { TERM_X, TERM_Y, TERM_MSG, TERM_CMD, TERM_INVALID } terminal_resourcetype;

typedef struct terminal_driver
{
  iot_logger_t * lc;
} terminal_driver;

static devsdk_address_t terminal_create_addr (void *impl, const devsdk_protocols *protocols, iot_data_t **exception)
{
  return (devsdk_address_t)protocols;
}

static void terminal_free_addr (void *impl, devsdk_address_t address)
{
}

static devsdk_resource_attr_t terminal_create_resource_attr (void *impl, const iot_data_t *attributes, iot_data_t **exception)
{
  terminal_resourcetype *attr = NULL;
  terminal_resourcetype result = TERM_INVALID;

  const char *param = iot_data_string_map_get_string (attributes, "parameter");
  if (param)
  {
    if (strcmp (param, "x") == 0)
    {
      result = TERM_X;
    }
    else if (strcmp (param, "y") == 0)
    {
      result = TERM_Y;
    }
    else if (strcmp (param, "msg") == 0)
    {
      result = TERM_MSG;
    }
    else if (strcmp (param, "cmd") == 0)
    {
      result = TERM_CMD;
    }
    else
    {
      *exception = iot_data_alloc_string ("terminal: invalid value specified for \"parameter\"", IOT_DATA_REF);
    }
  }
  else
  {
    *exception = iot_data_alloc_string ("terminal: \"parameter\" is required", IOT_DATA_REF);
  }
  if (result != TERM_INVALID)
  {
    attr = malloc (sizeof (terminal_resourcetype));
    *attr = result;
  }
  return attr;
}

static void terminal_free_resource_attr (void *impl, devsdk_resource_attr_t resource)
{
  free (resource);
}

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
    switch (*(terminal_resourcetype *)requests[i].resource->attrs)
    {
      case TERM_X:
        x = (iot_data_i32 (values[i])) % COLS;
        break;
      case TERM_Y:
        y = (iot_data_i32 (values[i])) % LINES;
        break;
      case TERM_MSG:
        msg = iot_data_string (values[i]);
        break;
      default:
        break;
    }
  }

  if (msg == NULL)
  {
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
  const devsdk_device_t *device,
  uint32_t nreadings,
  const devsdk_commandrequest * requests,
  devsdk_commandresult * readings,
  const iot_data_t *options,
  iot_data_t ** exception
)
{
  *exception = iot_data_alloc_string (ERR_TERMINAL_READ, IOT_DATA_REF);
  return false;
}

static bool terminal_put_handler
(
  void * impl,
  const devsdk_device_t *device,
  uint32_t nvalues,
  const devsdk_commandrequest * requests,
  const iot_data_t * values[],
  const iot_data_t *options,
  iot_data_t ** exception
)
{
  bool terminal_msg_state = false;
  terminal_driver * driver = (terminal_driver *) impl;
  char * buff;
  const char * command = NULL;

  for (uint32_t i = 0; i < nvalues; i++)
  {
    if (*(terminal_resourcetype *)requests[i].resource->attrs == TERM_CMD)
    {
      command = iot_data_string (values[i]);
      break;
    }
  }

  if (command == NULL)
  {
    *exception = iot_data_alloc_string (ERR_TERMINAL_NO_CMD, IOT_DATA_REF);
    return false;
  }
  else if (strcmp (command, "WriteMsg") == 0)
  {
    terminal_msg_state = terminal_writeMsg (driver, nvalues, requests, values);
    if (!terminal_msg_state)
    {
      *exception = iot_data_alloc_string (ERR_TERMINAL_MSG, IOT_DATA_REF);
      return false;
    }
    else
    {
      return true;
    }
  }
  else
  {
    buff = malloc (ERR_BUFSZ);
    snprintf (buff, ERR_BUFSZ, "Unknown command %s", command);
    *exception = iot_data_alloc_string (buff, IOT_DATA_TAKE);
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

  devsdk_callbacks *terminalImpls = devsdk_callbacks_init
  (
    terminal_init,
    terminal_get_handler,
    terminal_put_handler,
    terminal_stop,
    terminal_create_addr,
    terminal_free_addr,
    terminal_create_resource_attr,
    terminal_free_resource_attr
  );

  devsdk_service_t * service = devsdk_service_new
    ("device-terminal", "1.0", impl, terminalImpls, &argc, argv, &e);
  ERR_CHECK (e);

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Options:\n");
      printf ("  -h, --help\t\t\tShow this text\n");
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  devsdk_service_start (service, NULL, &e);
  ERR_CHECK (e);

  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigprocmask (SIG_BLOCK, &set, NULL);
  sigwait (&set, &sigret);
  sigprocmask (SIG_UNBLOCK, &set, NULL);

  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

  devsdk_service_free (service);
  free (impl);
  free (terminalImpls);
  return 0;
}
