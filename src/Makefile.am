# for debugging
#YELP_CFLAGS += -DYELP_DEBUG -pedantic -ansi -Werror

if ENABLE_MAN
man_sources = yelp-man-parser.c yelp-man-parser.h yelp-man-pager.c yelp-man-pager.h
else
man_sources =
endif

INCLUDES =								\
	-I$(top_srcdir)							\
	$(YELP_CFLAGS)							\
	$(MOZILLA_COMPONENT_CFLAGS)					\
	-I$(MOZILLA_INCLUDE_ROOT)					\
	-I$(MOZILLA_INCLUDE_ROOT)/dom					\
	-I$(MOZILLA_INCLUDE_ROOT)/find					\
	-I$(MOZILLA_INCLUDE_ROOT)/pref					\
	-I$(MOZILLA_INCLUDE_ROOT)/webbrwsr				\
	-I$(MOZILLA_INCLUDE_ROOT)/string				\
	-I$(MOZILLA_INCLUDE_ROOT)/webshell				\
	-I$(MOZILLA_INCLUDE_ROOT)/commandhandler			\
	-include $(MOZILLA_INCLUDE_ROOT)/mozilla-config.h		\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DIMAGEDIR=\""$(datadir)/images/yelp"\"				\
	-DSERVERDIR=\"$(libexecdir)\"					\
	-DBINDIR=\""$(bindir)"\"					\
	-DMOZILLA_HOME=\""$(MOZILLA_HOME)\""

bin_PROGRAMS = yelp
check_PROGRAMS = test-man-parser test-pager test-uri

yelp_LDFLAGS = -R$(MOZILLA_HOME)

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/gnome-help
	ln -s yelp $(DESTDIR)$(bindir)/gnome-help

test_man_parser_SOURCES =					\
	yelp-error.c		yelp-error.h			\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-man-parser.c	yelp-man-parser.h		\
	yelp-utils.c		yelp-utils.h			\
	test-man-parser.c

test_man_parser_LDADD = @YELP_LIBS@ $(Z_LIBS) $(BZ_LIBS)

test_pager_SOURCES =						\
	yelp-db-pager.c		yelp-db-pager.h			\
	yelp-error.c		yelp-error.h			\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-man-pager.c	yelp-man-pager.h		\
	yelp-man-parser.c	yelp-man-parser.h		\
	yelp-pager.c		yelp-pager.h			\
	yelp-toc-pager.c	yelp-toc-pager.h		\
	yelp-utils.c		yelp-utils.h			\
	yelp-marshal-main.c	test-pager.c

test_pager_LDADD = @YELP_LIBS@ $(POPT_LIBS) $(Z_LIBS) $(BZ_LIBS)

test_uri_SOURCES =					\
	yelp-utils.c	yelp-utils.h			\
	test-uri.c

test_uri_LDADD = @YELP_LIBS@

yelp_SOURCES =							\
	$(gnome_yelp_idl_sources)				\
	$(man_sources)						\
	yelp-base.c		yelp-base.h			\
	yelp-cache.c		yelp-cache.h			\
	yelp-db-pager.c		yelp-db-pager.h			\
	yelp-error.c		yelp-error.h			\
	yelp-html-gecko.c	yelp-html.h			\
	yelp-gecko-utils.cpp	yelp-gecko-utils.h		\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-pager.c		yelp-pager.h			\
	yelp-settings.c		yelp-settings.h			\
	yelp-toc-pager.c	yelp-toc-pager.h		\
	yelp-window.c 		yelp-window.h			\
	yelp-uri.c	        yelp-uri.h			\
	yelp-utils.c		yelp-utils.h			\
	yelp-marshal-main.c	yelp-main.c

yelp_LDADD = @YELP_LIBS@ $(POPT_LIBS) $(Z_LIBS) $(BZ_LIBS) $(MOZILLA_COMPONENT_LIBS)

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

distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

EXTRA_DIST =								\
	yelp-marshal.list						\
	$(server_in_files)						\
	$(server_DATA)
