YELP_COMMON_CFLAGS =				\
	$(YELP_CFLAGS)				\
	$(AM_CFLAGS)				\
	$(YELP_DEFINES)				\
	-I$(top_srcdir)/libyelp
YELP_COMMON_LDADD =				\
	$(YELP_LIBS)				\
	$(top_builddir)/libyelp/libyelp.la

check_PROGRAMS =				\
	test-location-entry			\
	test-uri				\
	test-view

test_location_entry_CFLAGS = $(YELP_COMMON_CFLAGS)
test_location_entry_LDADD = $(YELP_COMMON_LDADD)

test_uri_CFLAGS = $(YELP_COMMON_CFLAGS)
test_uri_LDADD = $(YELP_COMMON_LDADD)

test_view_CFLAGS = $(YELP_COMMON_CFLAGS)
test_view_LDADD = $(YELP_COMMON_LDADD)
