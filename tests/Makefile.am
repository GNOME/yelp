check_PROGRAMS = test-uri

test_uri_SOURCES =			\
	$(top_srcdir)/src/yelp-uri.c	\
	$(top_srcdir)/src/yelp-uri.h	\
	test-uri.c

test_uri_CFLAGS = $(YELP_CFLAGS) $(AM_CFLAGS) $(YELP_DEFINES) -I$(top_srcdir)/src `pkg-config --cflags gio-unix-2.0`
test_uri_LDADD = $(YELP_LIBS)
test_uri_LDFLAGS = $(AM_LDFLAGS) `pkg-config --libs gio-unix-2.0`
