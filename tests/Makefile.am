YELP_COMMON_CFLAGS =				\
	$(YELP_CFLAGS)				\
	$(AM_CFLAGS)				\
	$(YELP_DEFINES)				\
	-I$(top_srcdir)/libyelp
YELP_COMMON_LDADD =				\
	$(YELP_LIBS)				\
	$(top_builddir)/libyelp/libyelp.la

check_PROGRAMS = test-uri

test_uri_CFLAGS = $(YELP_COMMON_CFLAGS)
test_uri_LDADD = $(YELP_COMMON_LDADD)
