ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = po

BUILT_SOURCES = \
	$(nodist_libyelp_libyelp_la_SOURCES)

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
	libyelp/yelp-search-entry.c \
	libyelp/yelp-settings.c \
	libyelp/yelp-simple-document.c \
	libyelp/yelp-sqlite-storage.c \
	libyelp/yelp-storage.c \
	libyelp/yelp-transform.c \
	libyelp/yelp-uri.c \
	libyelp/yelp-uri-builder.c \
	libyelp/yelp-view.c

nodist_libyelp_libyelp_la_SOURCES = \
	libyelp/yelp-marshal.c \
	libyelp/yelp-types.c

noinst_libyelp_libyelp_la_headers = \
	libyelp/yelp-bz2-decompressor.h \
	libyelp/yelp-debug.h \
	libyelp/yelp-error.h \
	libyelp/yelp-info-parser.h \
	libyelp/yelp-man-parser.h \
	libyelp/yelp-lzma-decompressor.h \
	libyelp/yelp-magic-decompressor.h

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
	$(YELP_LIBS)

libyelp_libyelp_la_headers = \
	$(top_srcdir)/libyelp/yelp-bookmarks.h \
	$(top_srcdir)/libyelp/yelp-docbook-document.h \
	$(top_srcdir)/libyelp/yelp-document.h \
	$(top_srcdir)/libyelp/yelp-help-list.h \
	$(top_srcdir)/libyelp/yelp-info-document.h \
	$(top_srcdir)/libyelp/yelp-mallard-document.h \
	$(top_srcdir)/libyelp/yelp-man-document.h \
	$(top_srcdir)/libyelp/yelp-search-entry.h \
	$(top_srcdir)/libyelp/yelp-settings.h \
	$(top_srcdir)/libyelp/yelp-simple-document.h \
	$(top_srcdir)/libyelp/yelp-sqlite-storage.h \
	$(top_srcdir)/libyelp/yelp-storage.h \
	$(top_srcdir)/libyelp/yelp-transform.h \
	$(top_srcdir)/libyelp/yelp-uri.h \
	$(top_srcdir)/libyelp/yelp-uri-builder.h \
	$(top_srcdir)/libyelp/yelp-view.h

libyelp_includedir = $(includedir)/libyelp/

libyelp_include_HEADERS = $(libyelp_libyelp_la_headers)
nodist_libyelp_include_HEADERS = libyelp/yelp-types.h

marshal_data = \
	libyelp/yelp-marshal.c \
	libyelp/yelp-marshal.h

libyelp/yelp-marshal.h: libyelp/yelp-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=yelp_marshal --header --internal $< > $@

libyelp/yelp-marshal.c: libyelp/yelp-marshal.list libyelp/yelp-marshal.h
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=yelp_marshal --body --prototypes --internal $< > $@

enum_data = \
	libyelp/yelp-types.c \
	libyelp/yelp-types.h

libyelp/yelp-types.h: $(libyelp_libyelp_la_headers)
	$(AM_V_GEN) glib-mkenums \
	--fhead "#ifndef __LIBYELP_TYPES_H__\n" \
	--fhead "#define __LIBYELP_TYPES_H__\n\n" \
	--fhead "#include <glib-object.h>\n\n" \
	--fhead "G_BEGIN_DECLS\n\n" \
	--ftail "G_END_DECLS\n\n" \
	--ftail "#endif /* __LIBYELP_TYPES_H__ */\n" \
	--fprod "#include \"@filename@\"\n" \
	--eprod "#define YELP_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
	--eprod "GType @enum_name@_get_type (void);\n" \
	$(libyelp_libyelp_la_headers) > $@

libyelp/yelp-types.c: $(libyelp_libyelp_la_headers) libyelp/yelp-types.h
	$(AM_V_GEN) glib-mkenums \
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
	$(libyelp_libyelp_la_headers) > $@

webextension_LTLIBRARIES = libyelp/web-extension/libyelpwebextension.la
webextensiondir = $(pkglibdir)/web-extensions

libyelp_web_extension_libyelpwebextension_la_SOURCES = \
	libyelp/yelp-settings.c \
	libyelp/yelp-uri-builder.c \
	libyelp/yelp-uri.c \
	libyelp/web-extension/yelp-web-extension.c

libyelp_web_extension_libyelpwebextension_la_CFLAGS = \
	$(YELP_EXTENSION_CFLAGS)

libyelp_web_extension_libyelpwebextension_la_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	-DYELP_ICON_PATH=\"$(YELP_ICON_PATH)\" \
	-I$(top_srcdir)/libyelp

libyelp_web_extension_libyelpwebextension_la_LIBADD =  \
	$(YELP_EXTENSION_LIBS)

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
	$(WARN_CFLAGS) \
	-Wno-deprecated-declarations


yelp_CPPFLAGS = \
	-I$(top_srcdir)/libyelp \
	$(YELP_DEFINES)

yelp_LDADD = \
	libyelp/libyelp.la \
	$(YELP_LIBS)

yelp_LDFLAGS = $(WARN_LDFLAGS)

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
	tests/test-transform			\
	tests/test-uri				\
	tests/test-view

if ENABLE_LZMA
check_PROGRAMS += tests/test-lzma
tests_test_lzma_libyelp_sources = libyelp/yelp-lzma-decompressor.c
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

install-exec-hook:
	rm -f $(DESTDIR)$(bindir)/gnome-help && \
	$(LN_S) yelp $(DESTDIR)$(bindir)/gnome-help

desktopdir = $(datadir)/applications
desktop_DATA = yelp.desktop
yelp.desktop: ${srcdir}/yelp.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

data/yelp.appdata.xml: ${srcdir}/data/yelp.appdata.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

appstream_XML = data/yelp.appdata.xml
@APPSTREAM_XML_RULES@

gsettings_SCHEMAS = data/org.gnome.yelp.gschema.xml
@GSETTINGS_RULES@

16statusicondir = $(pkgdatadir)/icons/hicolor/16x16/status
scalablestatusicondir = $(pkgdatadir)/icons/hicolor/scalable/status
scalableappsicondir = $(datadir)/icons/hicolor/scalable/apps
symbolicappsicondir = $(datadir)/icons/hicolor/symbolic/apps

dist_16statusicon_DATA = \
	data/icons/hicolor/16x16/status/bookmark.png \
	data/icons/hicolor/16x16/status/yelp-page-task.png \
	data/icons/hicolor/16x16/status/yelp-page-tip.png \
	data/icons/hicolor/16x16/status/yelp-page-ui.png \
	data/icons/hicolor/16x16/status/yelp-page-video.png

dist_scalablestatusicon_DATA = \
	data/icons/hicolor/scalable/status/yelp-page-problem-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-search-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-task-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-tip-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-ui-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-video-symbolic.svg \
	data/icons/hicolor/scalable/status/yelp-page-symbolic.svg

dist_scalableappsicon_DATA = \
	data/icons/hicolor/scalable/apps/org.gnome.Yelp.svg

dist_symbolicappsicon_DATA = \
	data/icons/hicolor/symbolic/apps/org.gnome.Yelp-symbolic.svg

domaindir = $(datadir)/yelp-xsl/xslt/common/domains

data/domains/yelp.pot: data/domains/yelp.xml.in
	$(ITSTOOL) -o "$@" "$^"

data/domains/yelp.xml: data/domains/yelp.xml.in
	$(AM_V_GEN) ( \
	$(MKDIR_P) data/domains; \
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

mjdir = $(pkgdatadir)/mathjax

dist_mj_DATA = data/mathjax/MathJax.js

mjconfigdir = $(pkgdatadir)/mathjax/config

dist_mjconfig_DATA = \
	data/mathjax/config/MMLorHTML.js \
	data/mathjax/config/yelp.js

mjextensionsdir = $(pkgdatadir)/mathjax/extensions

dist_mjextensions_DATA = \
	data/mathjax/extensions/MathEvents.js \
	data/mathjax/extensions/MathMenu.js \
	data/mathjax/extensions/MathZoom.js \
	data/mathjax/extensions/mml2jax.js

mjextensionshtmlcssdir = $(pkgdatadir)/mathjax/extensions/HTML-CSS

dist_mjextensionshtmlcss_DATA = \
	data/mathjax/extensions/HTML-CSS/handle-floats.js

mjfontsdir = $(pkgdatadir)/mathjax/fonts/HTML-CSS/TeX/woff

dist_mjfonts_DATA = \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_AMS-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Caligraphic-Bold.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Caligraphic-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Fraktur-Bold.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Fraktur-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Main-Bold.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Main-Italic.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Main-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Math-BoldItalic.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Math-Italic.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Math-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_SansSerif-Bold.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_SansSerif-Italic.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_SansSerif-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Script-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Size1-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Size2-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Size3-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Size4-Regular.woff \
	data/mathjax/fonts/HTML-CSS/TeX/woff/MathJax_Typewriter-Regular.woff

mjelementsdir = $(pkgdatadir)/mathjax/jax/element/mml

dist_mjelements_DATA = \
	data/mathjax/jax/element/mml/jax.js

mjelementsoptabledir = $(pkgdatadir)/mathjax/jax/element/mml/optable

dist_mjelementsoptable_DATA = \
	data/mathjax/jax/element/mml/optable/Arrows.js \
	data/mathjax/jax/element/mml/optable/BasicLatin.js \
	data/mathjax/jax/element/mml/optable/CombDiacritMarks.js \
	data/mathjax/jax/element/mml/optable/CombDiactForSymbols.js \
	data/mathjax/jax/element/mml/optable/Dingbats.js \
	data/mathjax/jax/element/mml/optable/GeneralPunctuation.js \
	data/mathjax/jax/element/mml/optable/GeometricShapes.js \
	data/mathjax/jax/element/mml/optable/GreekAndCoptic.js \
	data/mathjax/jax/element/mml/optable/Latin1Supplement.js \
	data/mathjax/jax/element/mml/optable/LetterlikeSymbols.js \
	data/mathjax/jax/element/mml/optable/MathOperators.js \
	data/mathjax/jax/element/mml/optable/MiscMathSymbolsA.js \
	data/mathjax/jax/element/mml/optable/MiscMathSymbolsB.js \
	data/mathjax/jax/element/mml/optable/MiscSymbolsAndArrows.js \
	data/mathjax/jax/element/mml/optable/MiscTechnical.js \
	data/mathjax/jax/element/mml/optable/SpacingModLetters.js \
	data/mathjax/jax/element/mml/optable/SupplementalArrowsA.js \
	data/mathjax/jax/element/mml/optable/SupplementalArrowsB.js \
	data/mathjax/jax/element/mml/optable/SuppMathOperators.js

mkinputdir = $(pkgdatadir)/mathjax/jax/input/MathML

dist_mkinput_DATA = \
	data/mathjax/jax/input/MathML/config.js \
	data/mathjax/jax/input/MathML/jax.js

mkinputentitiesdir = $(pkgdatadir)/mathjax/jax/input/MathML/entities

dist_mkinputentities_DATA = \
	data/mathjax/jax/input/MathML/entities/a.js \
	data/mathjax/jax/input/MathML/entities/b.js \
	data/mathjax/jax/input/MathML/entities/c.js \
	data/mathjax/jax/input/MathML/entities/d.js \
	data/mathjax/jax/input/MathML/entities/e.js \
	data/mathjax/jax/input/MathML/entities/f.js \
	data/mathjax/jax/input/MathML/entities/fr.js \
	data/mathjax/jax/input/MathML/entities/g.js \
	data/mathjax/jax/input/MathML/entities/h.js \
	data/mathjax/jax/input/MathML/entities/i.js \
	data/mathjax/jax/input/MathML/entities/j.js \
	data/mathjax/jax/input/MathML/entities/k.js \
	data/mathjax/jax/input/MathML/entities/l.js \
	data/mathjax/jax/input/MathML/entities/m.js \
	data/mathjax/jax/input/MathML/entities/n.js \
	data/mathjax/jax/input/MathML/entities/o.js \
	data/mathjax/jax/input/MathML/entities/opf.js \
	data/mathjax/jax/input/MathML/entities/p.js \
	data/mathjax/jax/input/MathML/entities/q.js \
	data/mathjax/jax/input/MathML/entities/r.js \
	data/mathjax/jax/input/MathML/entities/scr.js \
	data/mathjax/jax/input/MathML/entities/s.js \
	data/mathjax/jax/input/MathML/entities/t.js \
	data/mathjax/jax/input/MathML/entities/u.js \
	data/mathjax/jax/input/MathML/entities/v.js \
	data/mathjax/jax/input/MathML/entities/w.js \
	data/mathjax/jax/input/MathML/entities/x.js \
	data/mathjax/jax/input/MathML/entities/y.js \
	data/mathjax/jax/input/MathML/entities/z.js

mjoutputhtmlcssdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS

dist_mjoutputhtmlcss_DATA = \
	data/mathjax/jax/output/HTML-CSS/config.js \
	data/mathjax/jax/output/HTML-CSS/jax.js

mjoutputautoloaddir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/autoload

dist_mjoutputautoload_DATA = \
	data/mathjax/jax/output/HTML-CSS/autoload/annotation-xml.js \
	data/mathjax/jax/output/HTML-CSS/autoload/maction.js \
	data/mathjax/jax/output/HTML-CSS/autoload/menclose.js \
	data/mathjax/jax/output/HTML-CSS/autoload/mglyph.js \
	data/mathjax/jax/output/HTML-CSS/autoload/mmultiscripts.js \
	data/mathjax/jax/output/HTML-CSS/autoload/ms.js \
	data/mathjax/jax/output/HTML-CSS/autoload/mtable.js \
	data/mathjax/jax/output/HTML-CSS/autoload/multiline.js

mjoutputfontsdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX

dist_mjoutputfonts_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/fontdata-extra.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/fontdata.js

mjoutputamsdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular

dist_mjoutputams_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Arrows.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/BBBold.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/BoxDrawing.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Dingbats.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/EnclosedAlphanum.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/GeneralPunctuation.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/GeometricShapes.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/GreekAndCoptic.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/LatinExtendedA.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Latin1Supplement.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/LetterlikeSymbols.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MathOperators.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MiscMathSymbolsB.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MiscSymbols.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MiscTechnical.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/PUA.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/SpacingModLetters.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/SuppMathOperators.js

mjoutputcalbolddir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Bold

dist_mjoutputcalbold_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Bold/Main.js

mjoutputcalregulardir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Regular

dist_mjoutputcalregular_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Regular/Main.js

mjoutputfrakturbolddir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold

dist_mjoutputfrakturbold_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/BasicLatin.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/Other.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/PUA.js

mjoutputfrakturregdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular

dist_mjoutputfrakturreg_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/BasicLatin.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/Other.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/PUA.js

mjoutputgreekbolddir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Bold

dist_mjoutputgreekbold_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Bold/Main.js

mjoutputgreekbolditalicdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/BoldItalic

dist_mjoutputgreekbolditalic_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/BoldItalic/Main.js

mjoutputgreekitalicdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Italic

dist_mjoutputgreekitalic_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Italic/Main.js

mjoutputgreekregdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Regular

dist_mjoutputgreekreg_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Regular/Main.js

mjoutputmainbolddir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold

dist_mjoutputmainbold_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/Arrows.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/CombDiactForSymbols.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/GeneralPunctuation.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/GeometricShapes.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/LatinExtendedA.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/LatinExtendedB.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/Latin1Supplement.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/LetterlikeSymbols.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MathOperators.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MiscMathSymbolsA.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MiscSymbols.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MiscTechnical.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/SpacingModLetters.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/SupplementalArrowsA.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/SuppMathOperators.js

mjoutputmainitalicdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Italic

dist_mjoutputmainitalic_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Italic/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Italic/GeneralPunctuation.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Italic/Latin1Supplement.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Italic/LetterlikeSymbols.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Italic/Main.js

mjoutputmainregdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Regular

dist_mjoutputmainreg_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Regular/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Regular/GeometricShapes.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Regular/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Regular/MiscSymbols.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Regular/SpacingModLetters.js

mjoutputmathbolddir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Math/BoldItalic

dist_mjoutputmathbold_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Math/BoldItalic/Main.js

mjoutputmathitalicdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Math/Italic

dist_mjoutputmathitalic_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Math/Italic/Main.js

mjoutputsanserifbolddir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold

dist_mjoutputsanserifbold_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/BasicLatin.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/Other.js

mjoutputsanserifitalicdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic

dist_mjoutputsanserifitalic_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/BasicLatin.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/Other.js

mjoutputsanserifregdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular

dist_mjoutputsanserifreg_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/BasicLatin.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/Other.js

mjoutputscriptdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Script/Regular

dist_mjoutputscript_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Script/Regular/BasicLatin.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Script/Regular/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Script/Regular/Other.js

mjoutputsize1dir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Size1/Regular

dist_mjoutputsize1_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size1/Regular/Main.js

mjoutputsize2dir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Size2/Regular

dist_mjoutputsize2_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size2/Regular/Main.js

mjoutputsize3dir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Size3/Regular

dist_mjoutputsize3_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size3/Regular/Main.js

mjoutputsize4dir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Size4/Regular

dist_mjoutputsize4_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size4/Regular/Main.js

mjoutputtypewriterdir = $(pkgdatadir)/mathjax/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular

dist_mjoutputtypewriter_DATA = \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/BasicLatin.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/CombDiacritMarks.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/Main.js \
	data/mathjax/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/Other.js

mjoutputnativemmldir = $(pkgdatadir)/mathjax/jax/output/NativeMML

dist_mjoutputnativemml_DATA = \
	data/mathjax/jax/output/NativeMML/config.js \
	data/mathjax/jax/output/NativeMML/jax.js

xsldir = $(pkgdatadir)/xslt

xsl_DATA = \
	data/xslt/db2html.xsl \
	data/xslt/info2html.xsl \
	data/xslt/links2html.xsl \
	data/xslt/mal2html.xsl \
	data/xslt/man2html.xsl \
	data/xslt/yelp-common.xsl

dist_noinst_DATA = \
	$(gsettings_SCHEMAS) \
	$(libyelp_libyelp_la_headers) \
	$(noinst_libyelp_libyelp_la_headers) \
	$(yelp_headers) \
	yelp.desktop.in \
	data/yelp.appdata.xml.in \
	data/domains/yelp.pot \
	data/domains/yelp.xml.in \
	data/domains/yelp.xml \
	data/mathjax/unpacked/MathJax.js \
	data/mathjax/unpacked/config/MMLorHTML.js \
	data/mathjax/unpacked/extensions/HTML-CSS/handle-floats.js \
	data/mathjax/unpacked/extensions/MathEvents.js \
	data/mathjax/unpacked/extensions/MathMenu.js \
	data/mathjax/unpacked/extensions/MathZoom.js \
	data/mathjax/unpacked/extensions/mml2jax.js \
	data/mathjax/unpacked/jax/element/mml/jax.js \
	data/mathjax/unpacked/jax/element/mml/optable/Arrows.js \
	data/mathjax/unpacked/jax/element/mml/optable/BasicLatin.js \
	data/mathjax/unpacked/jax/element/mml/optable/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/element/mml/optable/CombDiactForSymbols.js \
	data/mathjax/unpacked/jax/element/mml/optable/Dingbats.js \
	data/mathjax/unpacked/jax/element/mml/optable/GeneralPunctuation.js \
	data/mathjax/unpacked/jax/element/mml/optable/GeometricShapes.js \
	data/mathjax/unpacked/jax/element/mml/optable/GreekAndCoptic.js \
	data/mathjax/unpacked/jax/element/mml/optable/Latin1Supplement.js \
	data/mathjax/unpacked/jax/element/mml/optable/LetterlikeSymbols.js \
	data/mathjax/unpacked/jax/element/mml/optable/MathOperators.js \
	data/mathjax/unpacked/jax/element/mml/optable/MiscMathSymbolsA.js \
	data/mathjax/unpacked/jax/element/mml/optable/MiscMathSymbolsB.js \
	data/mathjax/unpacked/jax/element/mml/optable/MiscSymbolsAndArrows.js \
	data/mathjax/unpacked/jax/element/mml/optable/MiscTechnical.js \
	data/mathjax/unpacked/jax/element/mml/optable/SpacingModLetters.js \
	data/mathjax/unpacked/jax/element/mml/optable/SuppMathOperators.js \
	data/mathjax/unpacked/jax/element/mml/optable/SupplementalArrowsA.js \
	data/mathjax/unpacked/jax/element/mml/optable/SupplementalArrowsB.js \
	data/mathjax/unpacked/jax/input/MathML/config.js \
	data/mathjax/unpacked/jax/input/MathML/entities/a.js \
	data/mathjax/unpacked/jax/input/MathML/entities/b.js \
	data/mathjax/unpacked/jax/input/MathML/entities/c.js \
	data/mathjax/unpacked/jax/input/MathML/entities/d.js \
	data/mathjax/unpacked/jax/input/MathML/entities/e.js \
	data/mathjax/unpacked/jax/input/MathML/entities/f.js \
	data/mathjax/unpacked/jax/input/MathML/entities/fr.js \
	data/mathjax/unpacked/jax/input/MathML/entities/g.js \
	data/mathjax/unpacked/jax/input/MathML/entities/h.js \
	data/mathjax/unpacked/jax/input/MathML/entities/i.js \
	data/mathjax/unpacked/jax/input/MathML/entities/j.js \
	data/mathjax/unpacked/jax/input/MathML/entities/k.js \
	data/mathjax/unpacked/jax/input/MathML/entities/l.js \
	data/mathjax/unpacked/jax/input/MathML/entities/m.js \
	data/mathjax/unpacked/jax/input/MathML/entities/n.js \
	data/mathjax/unpacked/jax/input/MathML/entities/o.js \
	data/mathjax/unpacked/jax/input/MathML/entities/opf.js \
	data/mathjax/unpacked/jax/input/MathML/entities/p.js \
	data/mathjax/unpacked/jax/input/MathML/entities/q.js \
	data/mathjax/unpacked/jax/input/MathML/entities/r.js \
	data/mathjax/unpacked/jax/input/MathML/entities/s.js \
	data/mathjax/unpacked/jax/input/MathML/entities/scr.js \
	data/mathjax/unpacked/jax/input/MathML/entities/t.js \
	data/mathjax/unpacked/jax/input/MathML/entities/u.js \
	data/mathjax/unpacked/jax/input/MathML/entities/v.js \
	data/mathjax/unpacked/jax/input/MathML/entities/w.js \
	data/mathjax/unpacked/jax/input/MathML/entities/x.js \
	data/mathjax/unpacked/jax/input/MathML/entities/y.js \
	data/mathjax/unpacked/jax/input/MathML/entities/z.js \
	data/mathjax/unpacked/jax/input/MathML/jax.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/annotation-xml.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/maction.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/menclose.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/mglyph.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/mmultiscripts.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/ms.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/mtable.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/autoload/multiline.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/config.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Arrows.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/BBBold.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/BoxDrawing.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Dingbats.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/EnclosedAlphanum.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/GeneralPunctuation.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/GeometricShapes.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/GreekAndCoptic.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Latin1Supplement.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/LatinExtendedA.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/LetterlikeSymbols.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MathOperators.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MiscMathSymbolsB.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MiscSymbols.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/MiscTechnical.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/PUA.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/SpacingModLetters.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/SuppMathOperators.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Bold/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/BasicLatin.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/Other.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/PUA.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/BasicLatin.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/Other.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/PUA.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Greek/Bold/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Greek/BoldItalic/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Greek/Italic/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Greek/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/Arrows.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/CombDiactForSymbols.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/GeneralPunctuation.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/GeometricShapes.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/Latin1Supplement.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/LatinExtendedA.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/LatinExtendedB.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/LetterlikeSymbols.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MathOperators.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MiscMathSymbolsA.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MiscSymbols.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/MiscTechnical.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/SpacingModLetters.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/SuppMathOperators.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Bold/SupplementalArrowsA.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Italic/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Italic/Latin1Supplement.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Italic/LetterlikeSymbols.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Italic/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Regular/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Regular/GeometricShapes.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Regular/MiscSymbols.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Main/Regular/SpacingModLetters.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Math/BoldItalic/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Math/Italic/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/BasicLatin.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/Other.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/BasicLatin.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/Other.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/BasicLatin.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/Other.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Script/Regular/BasicLatin.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Script/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Script/Regular/Other.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Size1/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Size2/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Size3/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Size4/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/BasicLatin.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/CombDiacritMarks.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/Main.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/Other.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/fontdata.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/fonts/TeX/fontdata-extra.js \
	data/mathjax/unpacked/jax/output/HTML-CSS/jax.js \
	data/mathjax/unpacked/jax/output/NativeMML/config.js \
	data/mathjax/unpacked/jax/output/NativeMML/jax.js \
	data/mathjax/LICENSE \
	data/mathjax/README \
	libyelp/yelp-marshal.list

dist_noinst_SCRIPTS = \
	autogen.sh

CLEANFILES = \
	$(appstream_XML) \
	$(desktop_DATA) \
	$(enum_data) \
	$(marshal_data)

MAINTAINERCLEANFILES = \
	build-aux/compile \
	build-aux/config.guess \
	build-aux/config.sub \
	build-aux/depcomp \
	build-aux/install-sh \
	build-aux/ltmain.sh \
	build-aux/missing \
	build-aux/test-driver \
	aclocal.m4 \
	config.h.in \
	$(gsettings_SCHEMAS:.xml=.valid) \
	mkinstalldirs

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--with-webkit2gtk-4-0 \
	--enable-bz2 \
	--enable-lzma

.PHONY: docs
