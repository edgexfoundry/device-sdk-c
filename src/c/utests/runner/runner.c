#include "../../cunit/Basic.h"
#include "../../cunit/Automated.h"

#include "../base64/base64.h"

#include <stdbool.h>

static void usage (void)
{
  fprintf (stderr, "usage: runner [flags]\n");
  fprintf (stderr, "Legal flags:\n");
  fprintf (stderr, " -a run in automated mode\n");
  fprintf (stderr, " -r <file_name> results file for automated run\n");
  fprintf (stderr, " -j junit format results \n");
}

int main (int argc, char *argv[])
{
  int i;
  bool automated = false;
  bool junit = false;
  char *results = NULL;
  CU_BasicRunMode mode = CU_BRM_VERBOSE;
  CU_ErrorAction error_action = CUEA_IGNORE;

  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
        case 'a':
        {
          automated = true;
          break;
        }
        case 'j':
        {
          junit = true;
          break;
        }
        case 'r':
        {
          results = argv[++i];
          break;
        }
        default:
        {
          usage ();
          return -1;
        }
      }
    }
  }

  if (CU_initialize_registry ())
  {
    fprintf (stderr, "\nInitialization of C Unit Registry failed.");
    return -1;
  }

  cunit_base64_test_init ();

  CU_set_error_action (error_action);

  if (automated)
  {
    /* Generate CUnit or JUnit format results */

    if (results == NULL)
    {
      results = junit ? "junit" : "cunit";
    }
    CU_set_output_filename (results);

    if (junit)
    {
      CU_automated_enable_junit_xml (CU_TRUE);
    }
    else
    {
      CU_list_tests_to_file ();
    }

    CU_automated_run_tests ();
  }
  else
  {
    CU_basic_set_mode (mode);
    printf ("\nTests completed with return value %d.\n", CU_basic_run_tests ());
  }

  CU_cleanup_registry ();

  return 0;
}
