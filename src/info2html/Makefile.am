## Process this file with automake to produce Makefile.in.

INCLUDES = @YELP_CFLAGS@

libexec_PROGRAMS = yelp-info2html

yelp_info2html_SOURCES = main.c parse.c utils.c html.c data.h html.h parse.h utils.h version.h

yelp_info2html_LDADD = @YELP_LIBS@ $(Z_LIBS) $(BZ_LIBS)
