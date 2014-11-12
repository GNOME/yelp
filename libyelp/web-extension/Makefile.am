
webextension_LTLIBRARIES = libyelpwebextension.la
webextensiondir = $(pkglibdir)/web-extensions

libyelpwebextension_la_SOURCES = yelp-web-extension.c

libyelpwebextension_la_CFLAGS = \
	$(YELP_EXTENSION_CFLAGS)    \
	-I$(top_srcdir)/libyelp

libyelpwebextension_la_LIBADD =  \
	$(YELP_EXTENSION_LIBS)       \
	$(top_builddir)/libyelp/libyelpcommon.la

libyelpwebextension_la_LDFLAGS = -module -avoid-version -no-undefined