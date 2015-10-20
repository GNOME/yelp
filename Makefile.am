ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = po data docs

# Libtool is utterly broken when it comes to ordering library dependencies.
# Hack around this awfulness: https://lists.gnu.org/archive/html/libtool/2004-10/msg00051.html
# As libyelp depends on libyelpcommon, force libyelpcommon to be installed
# first.
install-exec-am: install-pkglibLTLIBRARIES install-libLTLIBRARIES install-binPROGRAMS
pkglib_LTLIBRARIES = libyelp/libyelpcommon.la

libyelp_libyelpcommon_la_SOURCES = \
	libyelp/yelp-uri.c \
	libyelp/yelp-uri-builder.c \
	libyelp/yelp-settings.c

libyelp_libyelpcommon_la_CFLAGS = \
	$(WARN_CFLAGS) \
	$(YELP_CFLAGS) \
	-Wno-deprecated-declarations

libyelp_libyelpcommon_la_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	-DYELP_ICON_PATH=\"$(YELP_ICON_PATH)\"

libyelp_libyelpcommon_la_LIBADD = $(YELP_LIBS)
libyelp_libyelpcommon_la_LDFLAGS = \
	-avoid-version \
	-no-undefined

libyelp_libyelpcommon_la_headers = \
	libyelp/yelp-uri.h \
	libyelp/yelp-uri-builder.h \
	libyelp/yelp-settings.h

libyelp_libyelpcommon_includedir = $(includedir)/libyelp/

libyelp_libyelpcommon_include_HEADERS = \
	$(libyelp_libyelpcommon_la_headers) \
	libyelp/yelp-common-types.h

BUILT_SOURCES = \
	libyelp/yelp-common-types.c \
	libyelp/yelp-common-types.h \
	libyelp/yelp-marshal.c \
	libyelp/yelp-marshal.h \
	libyelp/yelp-types.c \
	libyelp/yelp-types.h

libyelp/yelp-common-types.h: $(libyelp_libyelpcommon_la_headers)
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
	--fhead "#ifndef __LIBYELPCOMMON_TYPES_H__\n" \
	--fhead "#define __LIBYELPCOMMON_TYPES_H__\n\n" \
	--fhead "#include <glib-object.h>\n\n" \
	--fhead "G_BEGIN_DECLS\n\n" \
	--ftail "G_END_DECLS\n\n" \
	--ftail "#endif /* __LIBYELPCOMMON_TYPES_H__ */\n" \
	--fprod "#include \"@filename@\"\n" \
	--eprod "#define YELP_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
	--eprod "GType @enum_name@_get_type (void);\n" \
	$(libyelp_libyelpcommon_la_headers)) > $@

libyelp/yelp-common-types.c: $(libyelp_libyelpcommon_la_headers)
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
	--fhead "#include <glib-object.h>\n" \
	--fhead "#include \"yelp-types.h\"\n\n" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
	--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
	--vtail "  { 0, NULL, NULL }\n};\n\n" \
	--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
	--vtail "  static GType type = 0;\n\n" \
	--vtail "  if (!type)\n" \
	--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
	--vtail "  return type;\n}\n\n" \
	$(libyelp_libyelpcommon_la_headers)) > $@

lib_LTLIBRARIES = libyelp/libyelp.la

libyelp_libyelp_la_SOURCES = \
	libyelp/yelp-bookmarks.c \
	libyelp/yelp-debug.c \
	libyelp/yelp-error.c \
	libyelp/yelp-docbook-document.c \
	libyelp/yelp-document.c \
	libyelp/yelp-help-list.c \
	libyelp/yelp-info-document.c \
	libyelp/yelp-info-parser.c \
	libyelp/yelp-magic-decompressor.c \
	libyelp/yelp-mallard-document.c \
	libyelp/yelp-man-document.c \
	libyelp/yelp-man-parser.c \
	libyelp/yelp-marshal.c \
	libyelp/yelp-search-entry.c \
	libyelp/yelp-simple-document.c \
	libyelp/yelp-sqlite-storage.c \
	libyelp/yelp-storage.c \
	libyelp/yelp-transform.c \
	libyelp/yelp-types.c \
	libyelp/yelp-view.c

libyelp_libyelp_la_headers = \
	libyelp/yelp-bz2-decompressor.h \
	libyelp/yelp-debug.h \
	libyelp/yelp-error.h \
	libyelp/yelp-info-parser.h \
	libyelp/yelp-man-parser.h \
	libyelp/yelp-lzma-decompressor.h \
	libyelp/yelp-magic-decompressor.h \
	libyelp/yelp-marshal.list

if ENABLE_LZMA
libyelp_libyelp_la_SOURCES += libyelp/yelp-lzma-decompressor.c
endif

if ENABLE_BZ2
libyelp_libyelp_la_SOURCES += libyelp/yelp-bz2-decompressor.c
endif

libyelp_libyelp_la_CFLAGS = \
	$(WARN_CFLAGS) \
	$(YELP_CFLAGS) \
	-Wno-deprecated-declarations

libyelp_libyelp_la_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	-DYELP_ICON_PATH=\"$(YELP_ICON_PATH)\" \
	-DYELP_WEB_EXTENSIONS_DIR=\""$(pkglibdir)/"web-extensions\" \
	-I$(top_builddir)/libyelp

libyelp_libyelp_la_LIBADD = \
	$(LIBM) \
	$(YELP_LIBS) \
	libyelp/libyelpcommon.la

libyelp_libyelp_la_headers = \
	libyelp/yelp-bookmarks.h \
	libyelp/yelp-docbook-document.h \
	libyelp/yelp-document.h \
	libyelp/yelp-help-list.h \
	libyelp/yelp-info-document.h \
	libyelp/yelp-mallard-document.h \
	libyelp/yelp-man-document.h \
	libyelp/yelp-search-entry.h \
	libyelp/yelp-simple-document.h \
	libyelp/yelp-sqlite-storage.h \
	libyelp/yelp-storage.h \
	libyelp/yelp-transform.h \
	libyelp/yelp-view.h

libyelp_includedir = $(includedir)/libyelp/

libyelp_include_HEADERS = $(libyelp_libyelp_la_headers) libyelp/yelp-types.h

libyelp/yelp-marshal.h: libyelp/yelp-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=yelp_marshal $(top_srcdir)/libyelp/yelp-marshal.list --header --internal $< > $@

libyelp/yelp-marshal.c: libyelp/yelp-marshal.list libyelp/yelp-marshal.h
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=yelp_marshal $(top_srcdir)/libyelp/yelp-marshal.list --header --body --internal $< > $@

libyelp/yelp-types.h: $(libyelp_libyelp_la_headers)
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
	--fhead "#ifndef __LIBYELP_TYPES_H__\n" \
	--fhead "#define __LIBYELP_TYPES_H__\n\n" \
	--fhead "#include <glib-object.h>\n\n" \
	--fhead "G_BEGIN_DECLS\n\n" \
	--ftail "G_END_DECLS\n\n" \
	--ftail "#endif /* __LIBYELP_TYPES_H__ */\n" \
	--fprod "#include \"@filename@\"\n" \
	--eprod "#define YELP_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
	--eprod "GType @enum_name@_get_type (void);\n" \
	$(libyelp_libyelp_la_headers)) > $@

libyelp/yelp-types.c: $(libyelp_libyelp_la_headers) libyelp/yelp-types.h
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
	--fhead "#include <glib-object.h>\n" \
	--fhead "#include \"yelp-types.h\"\n\n" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
	--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
	--vtail "  { 0, NULL, NULL }\n};\n\n" \
	--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
	--vtail "  static GType type = 0;\n\n" \
	--vtail "  if (!type)\n" \
	--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
	--vtail "  return type;\n}\n\n" \
	$(libyelp_libyelp_la_headers)) > $@

webextension_LTLIBRARIES = libyelp/web-extension/libyelpwebextension.la
webextensiondir = $(pkglibdir)/web-extensions

libyelp_web_extension_libyelpwebextension_la_SOURCES = \
	libyelp/web-extension/yelp-web-extension.c

libyelp_web_extension_libyelpwebextension_la_CFLAGS = \
	$(YELP_EXTENSION_CFLAGS)

libyelp_web_extension_libyelpwebextension_la_CPPFLAGS = \
	-I$(top_srcdir)/libyelp

libyelp_web_extension_libyelpwebextension_la_LIBADD =  \
	$(YELP_EXTENSION_LIBS) \
	libyelp/libyelpcommon.la

libyelp_web_extension_libyelpwebextension_la_LDFLAGS = \
	-module \
	-avoid-version \
	-no-undefined

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

yelp_LDADD = \
	libyelp/libyelp.la \
	libyelp/libyelpcommon.la \
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
	libyelp/libyelp.la

tests_test_transform_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_transform_LDADD = \
	$(YELP_COMMON_LDADD) \
	libyelp/libyelp.la

tests_test_uri_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_uri_LDADD = \
	$(YELP_COMMON_LDADD) \
	libyelp/libyelp.la

tests_test_view_CFLAGS = $(YELP_COMMON_CFLAGS)
tests_test_view_LDADD = \
	$(YELP_COMMON_LDADD) \
	libyelp/libyelp.la

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files=yelp.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

@INTLTOOL_SCHEMAS_RULE@

gsettings_SCHEMAS = data/org.gnome.yelp.gschema.xml
@GSETTINGS_RULES@

16icondir = $(pkgdatadir)/icons/hicolor/16x16/status
scalableicondir = $(pkgdatadir)/icons/hicolor/scalable/status

dist_16icon_DATA = \
	data/icons/hicolor/16x16/status/bookmark.png \
	data/icons/hicolor/16x16/status/yelp-page-task.png \
	data/icons/hicolor/16x16/status/yelp-page-tip.png \
	data/icons/hicolor/16x16/status/yelp-page-ui.png \
	data/icons/hicolor/16x16/status/yelp-page-video.png

dist_scalableicon_DATA = \
	data/icons/hicolor/scalable/status/yelp-page-problem-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-search-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-task-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-tip-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-ui-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-video-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-symbolic.svg

domaindir = $(datadir)/yelp-xsl/xslt/common/domains

data/domains/yelp.pot: data/domains/yelp.xml.in
	$(ITSTOOL) -o "$@" "$^"

data/domains/yelp.xml: data/domains/yelp.xml.in
	$(AM_V_GEN) ( \
	for lang in $(ALL_LINGUAS); do \
	  msgfmt -o "$$lang.mo" "$(top_srcdir)/po/$$lang.po" || exit 1; \
	done; \
	$(ITSTOOL) -o "$@" -j "$^" $(foreach ll,$(ALL_LINGUAS),$(ll).mo); \
	rm $(foreach ll,$(ALL_LINGUAS),$(ll).mo))
	
domain_DATA = data/domains/yelp.xml

dtddir = $(pkgdatadir)/dtd

dist_dtd_DATA = \
	data/dtd/docbookx.dtd \
	data/dtd/isoamsa.ent \
	data/dtd/isoamsb.ent \
	data/dtd/isoamsc.ent \
	data/dtd/isoamsn.ent \
	data/dtd/isoamso.ent \
	data/dtd/isoamsr.ent \
	data/dtd/isobox.ent \
	data/dtd/isocyr1.ent \
	data/dtd/isocyr2.ent \
	data/dtd/isodia.ent \
	data/dtd/isogrk1.ent \
	data/dtd/isogrk2.ent \
	data/dtd/isogrk3.ent \
	data/dtd/isogrk4.ent \
	data/dtd/isolat1.ent \
	data/dtd/isolat2.ent \
	data/dtd/isonum.ent \
	data/dtd/isopub.ent \
	data/dtd/isotech.ent

dtd_DATA = \
	data/dtd/catalog

xsldir = $(pkgdatadir)/xslt

xsl_DATA = \
	data/xslt/db2html.xsl \
	data/xslt/info2html.xsl \
	data/xslt/mal2html.xsl \
	data/xslt/man2html.xsl \
	data/xslt/yelp-common.xsl

dist_noinst_DATA = \
	$(desktop_in_files) \
	$(gsettings_SCHEMAS) \
	$(libyelp_libyelp_la_headers) \
	$(yelp_headers) \
	data/domains/yelp.pot \
	data/domains/yelp.xml.in \
	data/domains/yelp.xml

dist_noinst_SCRIPTS = \
	autogen.sh

CLEANFILES = \
	$(desktop_DATA) \
	$(BUILT_SOURCES)

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-bz2 \
	--enable-gtk-doc \
	--enable-lzma
