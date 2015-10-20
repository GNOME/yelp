ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = libyelp libyelp/web-extension po data docs

bin_PROGRAMS = yelp

yelp_SOURCES = \
	src/yelp-application.c \
	src/yelp-window.c \
	src/yelp.c

yelp_headers = \
	src/yelp-application.h \
	src/yelp-window.h

YELP_DEFINES =								\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DDATADIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\"

yelp_CFLAGS =					\
	$(YELP_CFLAGS)				\
	$(WARN_CFLAGS)

yelp_CPPFLAGS = \
	-I$(top_srcdir)/libyelp \
	$(YELP_DEFINES)

yelp_LDADD =					\
	$(top_builddir)/libyelp/libyelp.la	\
	$(top_builddir)/libyelp/libyelpcommon.la	\
	$(YELP_LIBS)

yelp_LDFLAGS = $(WARN_LDFLAGS)

install-exec-hook:
	rm -f $(DESTDIR)$(bindir)/gnome-help && \
	$(LN_S) yelp $(DESTDIR)$(bindir)/gnome-help

YELP_COMMON_CFLAGS =				\
	$(YELP_CFLAGS)				\
	$(WARN_CFLAGS)				\
	$(YELP_DEFINES)				\
	-I$(top_srcdir)/libyelp
YELP_COMMON_LDADD =				\
	$(YELP_LIBS)				\
	$(WARN_LDFLAGS)

check_PROGRAMS =				\
	tests/test-magic			\
	tests/test-settings			\
	tests/test-transform			\
	tests/test-uri				\
	tests/test-view

if ENABLE_LZMA
check_PROGRAMS += tests/test-lzma
tests_test_lzma_libyelp_sources = $(top_srcdir)/libyelp/yelp-lzma-decompressor.c
tests_test_lzma_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_lzma_LDADD = $(YELP_COMMON_LDADD)
tests_test_lzma_SOURCES = tests/test-lzma.c $(tests_test_lzma_libyelp_sources)
endif

if ENABLE_BZ2
check_PROGRAMS += tests/test-bz2
tests_test_bz2_libyelp_sources = libyelp/yelp-bz2-decompressor.c
tests_test_bz2_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_bz2_LDADD = $(YELP_COMMON_LDADD)
tests_test_bz2_SOURCES = tests/test-bz2.c $(tests_test_bz2_libyelp_sources)
endif

tests_test_magic_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_magic_LDADD = $(YELP_COMMON_LDADD)
tests_test_magic_SOURCES = tests/test-magic.c \
	$(tests_test_bz2_libyelp_sources) \
	$(tests_test_lzma_libyelp_sources) \
	libyelp/yelp-magic-decompressor.c

tests_test_settings_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_settings_LDADD = \
	$(YELP_COMMON_LDADD) \
	$(top_builddir)/libyelp/libyelp.la

tests_test_transform_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_transform_LDADD = \
	$(YELP_COMMON_LDADD) \
	$(top_builddir)/libyelp/libyelp.la

tests_test_uri_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_uri_LDADD = \
	$(YELP_COMMON_LDADD) \
	$(top_builddir)/libyelp/libyelp.la

tests_test_view_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_view_LDADD = \
	$(YELP_COMMON_LDADD) \
	$(top_builddir)/libyelp/libyelp.la

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files=yelp.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

@INTLTOOL_SCHEMAS_RULE@

gsettings_SCHEMAS = data/org.gnome.yelp.gschema.xml
@GSETTINGS_RULES@

dist_noinst_DATA = \
	$(desktop_in_files) \
	$(gsettings_SCHEMAS) \
	$(yelp_headers)

dist_noinst_SCRIPTS = \
	autogen.sh

CLEANFILES = $(desktop_DATA)

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-bz2 \
	--enable-gtk-doc \
	--enable-lzma
