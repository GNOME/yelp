bin_PROGRAMS = yelp

yelp_SOURCES =							\
	$(gnome_yelp_idl_sources)				\
	yelp-base.c		yelp-base.h			\
	yelp-bookmarks.c 	yelp-bookmarks.h		\
	yelp-cache.c		yelp-cache.h			\
	yelp-db-pager.c		yelp-db-pager.h			\
	yelp-error.c		yelp-error.h			\
	yelp-gecko-utils.cpp	yelp-gecko-utils.h		\
	yelp-html-gecko.c	yelp-html.h			\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-pager.c		yelp-pager.h			\
	yelp-settings.c		yelp-settings.h			\
	yelp-toc-pager.c	yelp-toc-pager.h		\
	yelp-utils.c		yelp-utils.h			\
	yelp-window.c 		yelp-window.h			\
	yelp-xslt-pager.c	yelp-xslt-pager.h		\
	yelp-marshal-main.c	yelp-main.c

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

YELP_DEFINES =								\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DIMAGEDIR=\""$(datadir)/images/yelp"\"				\
	-DSERVERDIR=\"$(libexecdir)\"					\
	-DBINDIR=\""$(bindir)"\"					\
	-DMOZILLA_HOME=\""$(MOZILLA_HOME)\""

mozilla_include_subdirs =	\
	.		\
	dom		\
	find		\
	gtkembedmoz	\
	pref		\
	webbrwsr	\
	string		\
	webshell	\
	commandhandler

yelp_CPPFLAGS =									\
	-I$(top_srcdir)								\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(mozilla_include_subdirs))      \
	-include $(MOZILLA_INCLUDE_ROOT)/mozilla-config.h			\
	$(YELP_DEFINES)								\
	$(AM_CPPFLAGS)

yelp_CFLAGS =				\
	$(YELP_CFLAGS)			\
	$(AM_CFLAGS)

yelp_CXXFLAGS =				\
	$(YELP_CFLAGS)			\
	$(MOZILLA_COMPONENT_CFLAGS)	\
	$(AM_CXXFLAGS)

yelp_LDADD =				\
	$(YELP_LIBS)			\
	$(POPT_LIBS)			\
	$(Z_LIBS)			\
	$(BZ_LIBS)			\
	$(MOZILLA_COMPONENT_LIBS)

yelp_LDFLAGS = -R$(MOZILLA_HOME)

check_PROGRAMS = test-man-parser test-pager test-uri

test_man_parser_SOURCES =					\
	yelp-error.c		yelp-error.h			\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-man-parser.c	yelp-man-parser.h		\
	yelp-utils.c		yelp-utils.h			\
	test-man-parser.c

test_man_parser_CPPFLAGS =	\
	$(YELP_DEFINES)		\
	$(AM_CPPFLAGS)

test_man_parser_CFLAGS =	\
	$(YELP_CFLAGS)		\
	$(AM_CFLAGS)

test_man_parser_LDADD = $(YELP_LIBS) $(Z_LIBS) $(BZ_LIBS)

test_pager_SOURCES =						\
	yelp-db-pager.c		yelp-db-pager.h			\
	yelp-error.c		yelp-error.h			\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-man-pager.c	yelp-man-pager.h		\
	yelp-man-parser.c	yelp-man-parser.h		\
	yelp-pager.c		yelp-pager.h			\
	yelp-toc-pager.c	yelp-toc-pager.h		\
	yelp-utils.c		yelp-utils.h			\
	yelp-xslt-pager.c	yelp-xslt-pager.h		\
	yelp-marshal-main.c	test-pager.c

test_pager_CPPFLAGS =		\
	$(YELP_DEFINES)		\
	$(AM_CPPFLAGS)

test_pager_CFLAGS =		\
	$(YELP_CFLAGS)		\
	$(AM_CFLAGS)

test_pager_LDADD = $(YELP_LIBS) $(POPT_LIBS) $(Z_LIBS) $(BZ_LIBS)

test_uri_SOURCES =					\
	yelp-utils.c	yelp-utils.h			\
	test-uri.c

test_uri_CPPFLAGS =		\
	$(YELP_DEFINES)		\
	$(AM_CPPFLAGS)

test_uri_CFLAGS =		\
	$(YELP_CFLAGS)		\
	$(AM_CFLAGS)

test_uri_LDADD = $(YELP_LIBS)

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Yelp.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

BUILT_SOURCES = yelp-marshal.h yelp-marshal.c $(gnome_yelp_idl_sources)

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

gnome_yelp_idl_sources =						\
	GNOME_Yelp-stubs.c						\
	GNOME_Yelp-skels.c						\
	GNOME_Yelp-common.c						\
	GNOME_Yelp.h

$(gnome_yelp_idl_sources): $(top_srcdir)/idl/GNOME_Yelp.idl $(ORBIT_IDL)
	$(ORBIT_IDL) $(YELP_IDL_INCLUDES) -I$(top_srcdir) $<

CLEANFILES =								\
	$(gnome_yelp_idl_sources)					\
	gnome_yelp_idl_stamp						\
	GNOME_Yelp.server						\
	xgen-ymh xgen-ymc

MAINTAINERCLEANFILES = stamp-yelp-marshal.h $(BUILT_SOURCES)

EXTRA_DIST =								\
	yelp-marshal.list						\
	$(server_in_files)						\
	$(server_DATA)

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/gnome-help
	ln -s yelp $(DESTDIR)$(bindir)/gnome-help

distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi
