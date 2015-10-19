YELP_COMMON_CFLAGS =				\
	$(YELP_CFLAGS)				\
	$(WARN_CFLAGS)				\
	$(YELP_DEFINES)				\
	-DDATADIR=\""$(datadir)"\"		\
	-I$(top_srcdir)/libyelp
YELP_COMMON_LDADD =				\
	$(YELP_LIBS)				\
	$(WARN_LDFLAGS)

check_PROGRAMS =				\
	test-magic				\
	test-settings				\
	test-transform				\
	test-uri				\
	test-view

if ENABLE_LZMA
check_PROGRAMS += test-lzma
test_lzma_libyelp_sources = $(top_srcdir)/libyelp/yelp-lzma-decompressor.c
test_lzma_CFLAGS = $(YELP_COMMON_CFLAGS)
test_lzma_LDADD = $(YELP_COMMON_LDADD)
test_lzma_SOURCES = test-lzma.c $(test_lzma_libyelp_sources)
endif

if ENABLE_BZ2
check_PROGRAMS += test-bz2
test_bz2_libyelp_sources = $(top_srcdir)/libyelp/yelp-bz2-decompressor.c
test_bz2_CFLAGS = $(YELP_COMMON_CFLAGS)
test_bz2_LDADD = $(YELP_COMMON_LDADD)
test_bz2_SOURCES = test-bz2.c $(test_bz2_libyelp_sources)
endif

test_magic_CFLAGS = $(YELP_COMMON_CFLAGS)
test_magic_LDADD = $(YELP_COMMON_LDADD)
test_magic_SOURCES = test-magic.c \
	$(test_bz2_libyelp_sources) \
	$(test_lzma_libyelp_sources) \
	$(top_srcdir)/libyelp/yelp-magic-decompressor.c

test_settings_CFLAGS = $(YELP_COMMON_CFLAGS)
test_settings_LDADD = $(YELP_COMMON_LDADD) $(top_builddir)/libyelp/libyelp.la

test_transform_CFLAGS = $(YELP_COMMON_CFLAGS)
test_transform_LDADD = $(YELP_COMMON_LDADD) $(top_builddir)/libyelp/libyelp.la

test_uri_CFLAGS = $(YELP_COMMON_CFLAGS)
test_uri_LDADD = $(YELP_COMMON_LDADD) $(top_builddir)/libyelp/libyelp.la

test_view_CFLAGS = $(YELP_COMMON_CFLAGS)
test_view_LDADD = $(YELP_COMMON_LDADD) $(top_builddir)/libyelp/libyelp.la
