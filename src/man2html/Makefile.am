## Process this file with automake to produce Makefile.in.

INCLUDES = 								\
	-I$(top_srcdir)							\
	@YELP_CFLAGS@							


libexec_PROGRAMS = yelp-man2html

yelp_man2html_SOURCES = yelp-man2html.c

yelp_man2html_LDADD = 							\
	@YELP_LIBS@ $(Z_LIBS) $(BZ_LIBS)

#CFLAGS += -Wall

