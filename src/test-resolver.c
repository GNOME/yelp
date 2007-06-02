/* A test util to try out the new
 * yelp-utils resolver for URIs
 * Basically, tries to resolve each
 * given URI and compares to the expected results
 * (only checks the files exist and the type is correct)
 */

#include <stdio.h>
#include <glib.h>
#include "yelp-utils.h"


int DefaultTests (void);
int TestFile (char *uri);


int
TestFile (char *uri)
{

  return TRUE;
}

int
DefaultTests (void)
{

  return 0;
}

int
main (int argc, char *argv[])
{
  int i=1;

  if (argc % 2 != 1) {
    printf ("Usage: %s [<test-uri> <type> <test-uri> <type> ... ]\n", argv[0]);
    printf ("type can be one of:\n");
    printf ("man, info, doc, html, external, fail\n");
    return 1;
  }

  if (argc == 1) {
    int ret;
    ret = DefaultTests ();
    return ret;
  }
  while (i < argc) {
    char *uri = argv[i];
    char *type = argv[i+1];
    char *result = NULL;
    YelpSpoonType restype = YELP_TYPE_ERROR;


    printf ("uri: %s type: %s\n", argv[i], argv[i+1]);
    restype = yelp_uri_resolve (argv[i], &result);

    if (g_str_equal (type, "doc")) {
      if (restype != YELP_TYPE_DOC || !TestFile (result)) {
	printf ("Failed doc test %s.  Aborting.\n", uri);
	return 2;
      }
    } else if (g_str_equal (type, "info")) {
      if (restype != YELP_TYPE_INFO || !TestFile (result)) {
	printf ("Failed doc test %s.  Aborting.\n", uri);
	return 3;
      }
    } else if (g_str_equal (type, "man")) {
      if (restype != YELP_TYPE_MAN || !TestFile (result)) {
	printf ("Failed doc test %s.  Aborting.\n", uri);
	return 4;
      }
    } else if (g_str_equal (type, "external")) {
      if (restype != YELP_TYPE_EXTERNAL || !TestFile (result)) {
	printf ("Failed doc test %s.  Aborting.\n", uri, restype, YELP_TYPE_EXTERNAL);
	return 5;
      }
    } else if (g_str_equal (type, "error")) {
      if (restype != YELP_TYPE_ERROR || result != NULL) {
	printf ("Failed doc test %s.  Aborting.\n", uri);
	return 6;
      }
    } else {
      printf ("Unknown test: %s.  Ignoring.\n", type);
    }
    i+=2;
    g_free (result);
    result = NULL;
  }
  return 0;
}


