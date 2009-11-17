YELP_COMMON_CFLAGS =				\
	$(YELP_CFLAGS)				\
	$(AM_CFLAGS)				\
	$(YELP_DEFINES)				\
	-DDATADIR=\""$(datadir)"\"		\
	-I$(top_srcdir)/libyelp
YELP_COMMON_LDADD =				\
	$(YELP_LIBS)				\
	$(Z_LIBS)				\
	$(BZ_LIBS)				\
	$(LZMADEC_LIBS)				\
	$(top_builddir)/libyelp/libyelp.la

check_PROGRAMS =				\
	test-io-channel				\
	test-location-entry			\
	test-settings				\
	test-transform				\
	test-uri				\
	test-view

test_io_channel_CFLAGS = $(YELP_COMMON_CFLAGS)
test_io_channel_LDADD = $(YELP_COMMON_LDADD)

test_location_entry_CFLAGS = $(YELP_COMMON_CFLAGS)
test_location_entry_LDADD = $(YELP_COMMON_LDADD)

test_settings_CFLAGS = $(YELP_COMMON_CFLAGS)
test_settings_LDADD = $(YELP_COMMON_LDADD)

test_transform_CFLAGS = $(YELP_COMMON_CFLAGS)
test_transform_LDADD = $(YELP_COMMON_LDADD)

test_uri_CFLAGS = $(YELP_COMMON_CFLAGS)
test_uri_LDADD = $(YELP_COMMON_LDADD)

test_view_CFLAGS = $(YELP_COMMON_CFLAGS)
test_view_LDADD = $(YELP_COMMON_LDADD)
