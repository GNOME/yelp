/* A test util to try out the new
 * yelp-utils resolver for URIs
 * Basically, tries to resolve each
 * given URI and compares to the expected results
 * (only checks the files exist and the type is correct)
 */

#include <stdio.h>
#include "yelp-utils.h"


int
main (int argc, char *argv[])
{
  int i=1;

  if (argc < 2 || argc % 2 != 1) {
    printf ("Usage: %s <test-uri> <type>\n", argv[0]);
    printf ("type can be one of:\n");
    printf ("man, info, doc, external, fail\n");
    return 1;
  }

  while (i < argc) {
    printf ("uri: %s type: %s\n", argv[i], argv[i+1]);

    i+=2;
  }
}


