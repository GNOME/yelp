# Libtool is utterly broken when it comes to ordering library dependencies.
# Hack around this awfulness: https://lists.gnu.org/archive/html/libtool/2004-10/msg00051.html
# As libyelp depends on libyelpcommon, force libyelpcommon to be installed
# first.
install-exec-am: install-pkglibLTLIBRARIES install-libLTLIBRARIES
pkglib_LTLIBRARIES = libyelpcommon.la

libyelpcommon_la_SOURCES =      \
	yelp-uri.c                  \
	yelp-uri-builder.c          \
	yelp-settings.c

libyelpcommon_la_CFLAGS =               \
	$(WARN_CFLAGS)				\
	$(YELP_CFLAGS)                      \
	-Wno-deprecated-declarations		\
	-DDATADIR=\""$(datadir)"\"          \
	-DYELP_ICON_PATH=\"$(YELP_ICON_PATH)\"


libyelpcommon_la_LIBADD = $(YELP_LIBS)
libyelpcommon_la_LDFLAGS = 	\
	-avoid-version		\
	-no-undefined

libyelpcommon_headers =               \
	yelp-uri.h                  \
	yelp-uri-builder.h          \
	yelp-settings.h

libyelpcommon_includedir = $(includedir)/libyelp/

libyelpcommon_include_HEADERS = $(libyelpcommon_headers) yelp-common-types.h

BUILT_SOURCES =                  \
	yelp-common-types.c             \
	yelp-common-types.h

yelp-common-types.h: $(libyelpcommon_headers)
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
	$(libyelpcommon_headers)) > $@

yelp-common-types.c: $(libyelpcommon_headers)
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
	$(libyelpcommon_headers)) > $@

lib_LTLIBRARIES = libyelp.la

libyelp_la_SOURCES =                \
	yelp-bookmarks.c            \
	yelp-debug.c                \
	yelp-error.c                \
	yelp-docbook-document.c     \
	yelp-document.c             \
	yelp-help-list.c            \
	yelp-info-document.c        \
	yelp-info-parser.c          \
	yelp-magic-decompressor.c   \
	yelp-mallard-document.c     \
	yelp-man-document.c         \
	yelp-man-parser.c           \
	yelp-marshal.c              \
	yelp-search-entry.c         \
	yelp-simple-document.c      \
	yelp-sqlite-storage.c       \
	yelp-storage.c              \
	yelp-transform.c            \
	yelp-types.c                \
	yelp-view.c

EXTRA_DIST =                        \
	yelp-bz2-decompressor.h     \
	yelp-debug.h                \
	yelp-error.h                \
	yelp-info-parser.h          \
	yelp-man-parser.h           \
	yelp-lzma-decompressor.h    \
	yelp-magic-decompressor.h   \
	yelp-marshal.list

if ENABLE_LZMA
libyelp_la_SOURCES += yelp-lzma-decompressor.c
else
EXTRA_DIST += yelp-lzma-decompressor.c
endif

if ENABLE_BZ2
libyelp_la_SOURCES += yelp-bz2-decompressor.c
else
EXTRA_DIST += yelp-bz2-decompressor.c
endif

libyelp_la_CFLAGS =                             \
	$(WARN_CFLAGS)				\
	$(YELP_CFLAGS)                          \
	-Wno-deprecated-declarations		\
	-DDATADIR=\""$(datadir)"\"              \
	-DYELP_ICON_PATH=\"$(YELP_ICON_PATH)\"

libyelp_la_CPPFLAGS = \
	-DYELP_WEB_EXTENSIONS_DIR=\""$(pkglibdir)/"web-extensions\"

libyelp_la_LIBADD =  \
	$(YELP_LIBS)     \
	libyelpcommon.la

libyelp_headers =                   \
	yelp-bookmarks.h            \
	yelp-docbook-document.h     \
	yelp-document.h             \
	yelp-help-list.h            \
	yelp-info-document.h        \
	yelp-mallard-document.h     \
	yelp-man-document.h         \
	yelp-search-entry.h         \
	yelp-simple-document.h      \
	yelp-sqlite-storage.h       \
	yelp-storage.h              \
	yelp-transform.h            \
	yelp-view.h

libyelp_includedir = $(includedir)/libyelp/

libyelp_include_HEADERS = $(libyelp_headers) yelp-types.h

BUILT_SOURCES +=                 \
	yelp-marshal.c           \
	yelp-marshal.h           \
	yelp-types.c             \
	yelp-types.h

yelp-marshal.h: yelp-marshal.list
	$(AM_V_GEN) glib-genmarshal --prefix=yelp_marshal $(srcdir)/yelp-marshal.list --header $< > $@

yelp-marshal.c: yelp-marshal.list
	$(AM_V_GEN) glib-genmarshal --prefix=yelp_marshal $(srcdir)/yelp-marshal.list --header --body $< > $@

CLEANFILES = $(BUILT_SOURCES)

yelp-types.h: $(libyelp_headers)
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
	$(libyelp_headers)) > $@

yelp-types.c: $(libyelp_headers)
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
	$(libyelp_headers)) > $@
