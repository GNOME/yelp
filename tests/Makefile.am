YELP_COMMON_CFLAGS =				\
	$(YELP_CFLAGS)				\
	$(AM_CFLAGS)				\
	$(YELP_DEFINES)				\
	-DDATADIR=\""$(datadir)"\"		\
	-I$(top_srcdir)/libyelp
YELP_COMMON_LDADD =				\
	$(YELP_LIBS)				\
	$(top_builddir)/libyelp/libyelp.la

check_PROGRAMS =				\
	test-magic				\
	test-settings				\
	test-transform				\
	test-uri				\
	test-view
#	test-location-entry

if ENABLE_LZMA
check_PROGRAMS += test-lzma
test_lzma_CFLAGS = $(YELP_COMMON_CFLAGS)
test_lzma_LDADD = $(YELP_COMMON_LDADD)
endif

if ENABLE_BZ2
check_PROGRAMS += test-bz2
test_bz2_CFLAGS = $(YELP_COMMON_CFLAGS)
test_bz2_LDADD = $(YELP_COMMON_LDADD)
endif

test_location_entry_CFLAGS = $(YELP_COMMON_CFLAGS)
test_location_entry_LDADD = $(YELP_COMMON_LDADD)

test_magic_CFLAGS = $(YELP_COMMON_CFLAGS)
test_magic_LDADD = $(YELP_COMMON_LDADD)

test_settings_CFLAGS = $(YELP_COMMON_CFLAGS)
test_settings_LDADD = $(YELP_COMMON_LDADD)

test_transform_CFLAGS = $(YELP_COMMON_CFLAGS)
test_transform_LDADD = $(YELP_COMMON_LDADD)

test_uri_CFLAGS = $(YELP_COMMON_CFLAGS)
test_uri_LDADD = $(YELP_COMMON_LDADD)

test_view_CFLAGS = $(YELP_COMMON_CFLAGS)
test_view_LDADD = $(YELP_COMMON_LDADD)
