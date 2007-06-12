/* A test util to try out the new
 * yelp-utils resolver for URIs
 * Basically, tries to resolve each
 * given URI and compares to the expected results
 * (only checks the files exist and the type is correct)
 */

#include <stdio.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include "yelp-utils.h"


int DefaultTests (void);
int TestFile (char *uri);


int
TestFile (char *uri)
{
  return (g_file_test (uri, G_FILE_TEST_EXISTS)); 
}

int
DefaultTests (void)
{
  gchar *result = NULL;
  gchar *section = NULL;
  YelpSpoonType restype = YELP_TYPE_ERROR;

  /* First, normal docs - these will only work with spoon XDG_DATA_DIRS set correctly */
  /* Normal doc, no section */
  restype = yelp_uri_resolve ("ghelp:user-guide", &result, &section);
  if (restype != YELP_TYPE_DOC || !TestFile (result) ||
      section != NULL) {
    return 101;
  }
  g_free (result); result=NULL;

  /* Section type 1*/
  restype = yelp_uri_resolve ("ghelp:user-guide#madeupsection", &result, &section);
  if (restype != YELP_TYPE_DOC || !TestFile (result) ||
      !g_str_equal (section, "madeupsection")) {
    return 102;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* Section type 2 */
  restype = yelp_uri_resolve ("ghelp:user-guide?madeupsection", &result, &section);
  if (restype != YELP_TYPE_DOC || !TestFile (result) ||
      !g_str_equal (section, "madeupsection")) {
    return 103;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;
    
  /* man pages - only work with correct man pages installed */
  /* Simple man page */
  restype = yelp_uri_resolve ("man:yelp", &result, &section);
  if (restype != YELP_TYPE_MAN || !TestFile (result) ||
      !g_str_equal (section, "1")) {
    return 104;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* man page from specific section 1*/
  restype = yelp_uri_resolve ("man:yelp(1)", &result, &section);
  if (restype != YELP_TYPE_MAN || !TestFile (result) ||
      !g_str_equal (section, "1")) {
    return 105;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* man page from specific section 2*/
  restype = yelp_uri_resolve ("man:yelp.1", &result, &section);
  if (restype != YELP_TYPE_MAN || !TestFile (result) ||
      !g_str_equal (section, "1")) {
    return 106;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* man page from specific section 3*/
  restype = yelp_uri_resolve ("man:yelp#1", &result, &section);
  if (restype != YELP_TYPE_MAN || !TestFile (result) ||
      !g_str_equal (section, "1")) {
    return 107;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* Info pages */
  /* Simple info page */
  restype = yelp_uri_resolve ("info:cvs", &result, &section);
  if (restype != YELP_TYPE_INFO || !TestFile (result) ||
      section != NULL) {
    return 108;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* info page with section */
  restype = yelp_uri_resolve ("info:cvs#toolbar", &result, &section);
  if (restype != YELP_TYPE_INFO || !TestFile (result) ||
      !g_str_equal (section, "toolbar")) {
    return 109;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* info page with section 2*/
  restype = yelp_uri_resolve ("info:cvs?toolbar", &result, &section);
  if (restype != YELP_TYPE_INFO || !TestFile (result) ||
      !g_str_equal (section, "toolbar")) {
    return 110;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;


  /* info page with section included */
  restype = yelp_uri_resolve ("info:autopoint", &result, &section);
  if (restype != YELP_TYPE_INFO || !TestFile (result) ||
      !g_str_equal (section, "autopoint Invocation")) {
    return 111;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* Other types: html - no html installed by default.  Should be the same
   * as ghelp 
   */
  /* External */
  restype = yelp_uri_resolve ("http://www.gnome.org", &result, &section);
  if (restype != YELP_TYPE_EXTERNAL) {
    return 112;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;
  
  /* External, but local */
  restype = yelp_uri_resolve ("/usr/bin/yelp", &result, &section);
  if (restype != YELP_TYPE_EXTERNAL) {
    return 113;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* External, local using file: uri */
  restype = yelp_uri_resolve ("file:///usr/bin/yelp", &result, &section);
  if (restype != YELP_TYPE_EXTERNAL) {
    return 114;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* Local, file, readable */
  restype = yelp_uri_resolve ("file:///usr/share/gnome/help/user-guide/C/user-guide.xml", &result, &section);
  if (restype != YELP_TYPE_DOC  || !TestFile (result) ||
      section != NULL) {
    return 115;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* Local, readable, html */
  restype = yelp_uri_resolve ("/usr/share/doc/shared-mime-info/shared-mime-info-spec.html/index.html", &result, &section);
  if (restype != YELP_TYPE_HTML || !TestFile (result) ||
      section != NULL) {
    return 116;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* Local, readable, html, with section */
  restype = yelp_uri_resolve ("/usr/share/doc/shared-mime-info/shared-mime-info-spec.html/index.html#foobar", &result, &section);
  if (restype != YELP_TYPE_HTML || !TestFile (result) ||
      !g_str_equal (section, "foobar")) {
    return 117;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;

  /* error */
  restype = yelp_uri_resolve ("file:///usr/fake_file1", &result, &section);
  if (restype != YELP_TYPE_ERROR || result != NULL || section != NULL) {
    return 118;
  }
  g_free (result); result=NULL;
  g_free (section); section = NULL;




  return 0;
}

int
main (int argc, char *argv[])
{
  int i=1;

  /* Used within yelp-utils */
  gnome_vfs_init ();

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
    char *section = NULL;
    YelpSpoonType restype = YELP_TYPE_ERROR;


    printf ("uri: %s type: %s\n", argv[i], argv[i+1]);

    if (g_str_equal (type, "doc")) {
      restype = yelp_uri_resolve (argv[i], &result, &section);
      if (restype != YELP_TYPE_DOC || !TestFile (result)) {
	printf ("Failed doc test %s.  Aborting.\n", uri);
	return 2;
      }
    } else if (g_str_equal (type, "info")) {
      restype = yelp_uri_resolve (argv[i], &result, &section);
      if (restype != YELP_TYPE_INFO || !TestFile (result)) {
	printf ("Failed doc test %s.  Aborting.\n", uri);
	return 3;
      }
    } else if (g_str_equal (type, "man")) {
      restype = yelp_uri_resolve (argv[i], &result, &section);
      if (restype != YELP_TYPE_MAN || !TestFile (result)) {
	printf ("Failed doc test %s.  Aborting.\n", uri);
	return 4;
      }
    } else if (g_str_equal (type, "external")) {
      restype = yelp_uri_resolve (argv[i], &result, &section);
      if (restype != YELP_TYPE_EXTERNAL) {
	printf ("Failed doc test %s.  Aborting.\n", uri, restype, YELP_TYPE_EXTERNAL);
	return 5;
      }
    } else if (g_str_equal (type, "error")) {
      restype = yelp_uri_resolve (argv[i], &result, &section);
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


