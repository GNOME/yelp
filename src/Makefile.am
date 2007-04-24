bin_PROGRAMS = yelp

yelp_SOURCES =							\
	Yelper.cpp		Yelper.h			\
	yelp-base.c		yelp-base.h			\
	yelp-bookmarks.c 	yelp-bookmarks.h		\
	yelp-db-pager.c		yelp-db-pager.h			\
	yelp-db-print-pager.c	yelp-db-print-pager.h		\
	yelp-debug.c		yelp-debug.h			\
	yelp-error.c		yelp-error.h			\
	yelp-gecko-utils.cpp	yelp-gecko-utils.h		\
	yelp-html.cpp		yelp-html.h			\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-pager.c		yelp-pager.h			\
	yelp-settings.c		yelp-settings.h			\
	yelp-toc-pager.c	yelp-toc-pager.h		\
	yelp-utils.c		yelp-utils.h			\
	yelp-window.c 		yelp-window.h			\
	yelp-xslt-pager.c	yelp-xslt-pager.h		\
	yelp-marshal.c		yelp-marshal.h			\
	yelp-main.c						\
	yelp-print.c		yelp-print.h			\
	yelp-gecko-services.h   yelp-gecko-services.cpp	

if ENABLE_MAN
yelp_SOURCES +=					\
	yelp-man-parser.c yelp-man-parser.h	\
	yelp-man-pager.c yelp-man-pager.h
endif

if ENABLE_INFO
yelp_SOURCES +=					\
	yelp-info-pager.c yelp-info-pager.h	\
	yelp-info-parser.c yelp-info-parser.h
endif

if ENABLE_SEARCH
yelp_SOURCES +=						\
	gtkentryaction.c	gtkentryaction.h	\
	yelp-search-pager.c	yelp-search-pager.h
endif

YELP_DEFINES =								\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DIMAGEDIR=\""$(datadir)/images/yelp"\"				\
	-DSERVERDIR=\"$(libexecdir)\"					\
	-DBINDIR=\""$(bindir)"\"					\
	-DSHAREDIR=\""$(pkgdatadir)"\"                                  \
	-DMOZILLA_HOME=\""$(MOZILLA_HOME)\""

mozilla_include_subdirs =	\
	.		\
	commandhandler  \
	content		\
	docshell	\
	dom		\
	fastfind	\
	find		\
	gtkembedmoz	\
	gfx		\
	layout		\
	necko		\
	pref		\
	string		\
	uriloader	\
	webbrwsr	\
	webshell	\
	widget

yelp_CPPFLAGS =									\
	-I$(top_srcdir)								\
	-I$(top_builddir)/src							\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(mozilla_include_subdirs))      \
	$(YELP_DEFINES)								\
	$(AM_CPPFLAGS)

yelp_CFLAGS =				\
	$(YELP_CFLAGS)			\
	$(YELP_SEARCH_CFLAGS)		\
	$(AM_CFLAGS)

yelp_CXXFLAGS =				\
	$(YELP_CFLAGS)			\
	$(YELP_SEARCH_CFLAGS)		\
	$(MOZILLA_COMPONENT_CFLAGS)	\
	$(AM_CXXFLAGS)

yelp_LDADD =				\
	$(YELP_LIBS)			\
	$(YELP_SEARCH_LIBS)		\
	$(POPT_LIBS)			\
	$(Z_LIBS)			\
	$(BZ_LIBS)			\
	$(X_LIBS)			\
	$(MOZILLA_COMPONENT_LIBS)	\
	$(MOZILLA_EXTRA_LIBS)		\
	$(MOZILLA_GLUE_LIBS)

yelp_LDFLAGS = -R$(MOZILLA_HOME) $(AM_LDFLAGS)

check_PROGRAMS =		\
	test-document		\
	test-man-parser		\
	test-page		\
	test-transform

test_document_SOURCES =					\
	yelp-debug.c		yelp-debug.h		\
	yelp-docbook.c		yelp-docbook.h		\
	yelp-document.c		yelp-document.h		\
	yelp-error.c		yelp-error.h		\
	yelp-io-channel.c	yelp-io-channel.h	\
	yelp-man.c		yelp-man.h		\
	yelp-man-parser.c	yelp-man-parser.h	\
	yelp-page.c		yelp-page.h		\
	yelp-toc.c		yelp-toc.h		\
	yelp-transform.c	yelp-transform.h	\
	test-document.c
test_document_CFLAGS = $(YELP_CFLAGS) $(AM_CFLAGS) $(YELP_DEFINES)
test_document_LDADD = $(YELP_LIBS) $(Z_LIBS) $(BZ_LIBS)
test_document_LDFLAGS = $(AM_LDFLAGS)

test_man_parser_SOURCES =					\
	yelp-debug.c		yelp-debug.h			\
	yelp-error.c		yelp-error.h			\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-man-parser.c	yelp-man-parser.h		\
	test-man-parser.c
test_man_parser_CPPFLAGS = $(YELP_DEFINES) $(AM_CPPFLAGS)
test_man_parser_CFLAGS = $(YELP_CFLAGS) $(AM_CFLAGS)
test_man_parser_LDADD = $(YELP_LIBS) $(Z_LIBS) $(BZ_LIBS)
test_man_parser_LDFLAGS = $(AM_LDFLAGS)

test_page_SOURCES =					\
	yelp-debug.c		yelp-debug.h		\
	yelp-document.c		yelp-document.h		\
	yelp-error.c		yelp-error.h		\
	yelp-page.c		yelp-page.h		\
	test-page.c
test_page_CFLAGS = $(YELP_CFLAGS) $(AM_CFLAGS) $(YELP_DEFINES)
test_page_LDADD = $(YELP_LIBS)
test_page_LDFLAGS = $(AM_LDFLAGS)

test_transform_SOURCES =				\
	yelp-debug.c		yelp-debug.h		\
	yelp-error.c		yelp-error.h		\
	yelp-transform.c	yelp-transform.h	\
	test-transform.c
test_transform_CFLAGS = $(YELP_CFLAGS) $(AM_CFLAGS) $(YELP_DEFINES)
test_transform_LDADD = $(YELP_LIBS)
test_transform_LDFLAGS = $(AM_LDFLAGS)


@INTLTOOL_SERVER_RULE@

BUILT_SOURCES = yelp-marshal.h stamp-yelp-marshal.h yelp-marshal.c server-bindings.h \
		client-bindings.h

noinst_HEADERS = $(BUILT_SOURCES)

server-bindings.h:  yelp-infos.xml
	dbus-binding-tool --prefix=server_object --mode=glib-server $< > $@

client-bindings.h:  yelp-infos.xml
	dbus-binding-tool --prefix=server_object --mode=glib-client $< > $@

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

CLEANFILES = xgen-ymh xgen-ymc

MAINTAINERCLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = yelp-marshal.list yelp-infos.xml

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/gnome-help && \
	$(LN_S) yelp $(DESTDIR)$(bindir)/gnome-help

distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi
