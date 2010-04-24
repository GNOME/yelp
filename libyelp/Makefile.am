lib_LTLIBRARIES = libyelp.la

libyelp_la_SOURCES =				\
	yelp-debug.c				\
	yelp-error.c				\
	yelp-docbook-document.c			\
	yelp-document.c				\
	yelp-io-channel.c			\
	yelp-location-entry.c			\
	yelp-mallard-document.c			\
	yelp-marshal.c				\
	yelp-settings.c				\
	yelp-simple-document.c			\
	yelp-transform.c			\
	yelp-uri.c				\
	yelp-types.c				\
	yelp-view.c

libyelp_la_CFLAGS =				\
	$(YELP_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"		\
	-DYELP_ICON_PATH=\"$(YELP_ICON_PATH)\"

libyelp_la_LIBADD =				\
	$(YELP_LIBS)

libyelp_headers =				\
	yelp-docbook-document.h			\
	yelp-document.h				\
	yelp-location-entry.h			\
	yelp-mallard-document.h			\
	yelp-settings.h				\
	yelp-simple-document.h			\
	yelp-transform.h			\
	yelp-uri.h				\
	yelp-view.h

libyelp_includedir = $(includedir)/libyelp/

libyelp_include_HEADERS =			\
	$(libyelp_headers)			\
	yelp-types.h

BUILT_SOURCES =			\
	stamp-yelp-marshal.h	\
	yelp-marshal.c		\
	yelp-marshal.h		\
	yelp-types.c		\
	yelp-types.h

EXTRA_DIST =			\
	yelp-debug.h		\
	yelp-error.h		\
	yelp-io-channel.h	\
	yelp-marshal.list

yelp-marshal.h: stamp-yelp-marshal.h
	@true

stamp-yelp-marshal.h: yelp-marshal.list
	@GLIB_GENMARSHAL@ --prefix=yelp_marshal $(srcdir)/yelp-marshal.list --header > xgen-ymh \
	&& (cmp -s xgen-ymh yelp-marshal.h || cp xgen-ymh yelp-marshal.h) \
	&& rm -f xgen-ymh xgen-ymh~ \
	&& echo timestamp > $(@F)

yelp-marshal.c: yelp-marshal.list
	@GLIB_GENMARSHAL@ --prefix=yelp_marshal $(srcdir)/yelp-marshal.list --body > xgen-ymc \
	&& cp xgen-ymc yelp-marshal.c \
	&& rm -f xgen-ymc xgen-ymc~

yelp-marshal-main.c: yelp-marshal.c

CLEANFILES = $(BUILT_SOURCES)

yelp-types.h: $(libyelp_headers)
	(cd $(srcdir) && glib-mkenums \
	--fhead "#ifndef __LIBYELP_TYPES_H__\n" \
	--fhead "#define __LIBYELP_TYPES_H__\n\n" \
	--fhead "#include <glib-object.h>\n\n" \
	--fhead "G_BEGIN_DECLS\n\n" \
	--ftail "G_END_DECLS\n\n" \
	--ftail "#endif /* __LIBYELP_TYPES_H__ */\n" \
	--fprod "#include <libyelp/@filename@>\n" \
	--eprod "#define YELP_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
	--eprod "GType @enum_name@_get_type (void);\n" \
	$(libyelp_headers)) > $@

yelp-types.c: $(libyelp_headers)
	(cd $(srcdir) && glib-mkenums \
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
