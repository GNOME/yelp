# for debugging
YELP_CFLAGS += -DYELP_DEBUG
# -pedantic -ansi -Werror

INCLUDES =								\
	-I$(top_srcdir)							\
	$(YELP_CFLAGS)							\
	$(MOZILLA_COMPONENT_CFLAGS)					\
	-I$(MOZILLA_INCLUDE_ROOT)					\
	-I$(MOZILLA_INCLUDE_ROOT)/dom					\
	-I$(MOZILLA_INCLUDE_ROOT)/find					\
	-I$(MOZILLA_INCLUDE_ROOT)/webbrwsr				\
	-I$(MOZILLA_INCLUDE_ROOT)/string				\
	-include $(MOZILLA_INCLUDE_ROOT)/mozilla-config.h		\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DIMAGEDIR=\""$(datadir)/images/yelp"\"				\
	-DGNOMELOCALEDIR=\""$(yelplocaledir)"\"				\
	-DLIBGNOME_MAN2HTML=\"$(LIBGNOME_MAN2HTML)\"			\
	-DLIBGNOME_INFO2HTML=\"$(LIBGNOME_INFO2HTML)\"			\
	-DSERVERDIR=\"$(libexecdir)\"					\
	-DBINDIR=\""$(bindir)"\"

bin_PROGRAMS = yelp
check_PROGRAMS = test-man-parser test-pager test-uri

yelp_LDFLAGS = -R$(MOZILLA_HOME)

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/gnome-help
	ln -s yelp $(DESTDIR)$(bindir)/gnome-help

yelp-marshal.h: yelp-marshal.list
	cd $(srcdir) && \
	( @GLIB_GENMARSHAL@ --prefix=yelp_marshal yelp-marshal.list --header > yelp-marshal1.tmp \
	&& mv yelp-marshal1.tmp yelp-marshal.h ) \
	|| ( rm -f yelp-marshal1.tmp && exit 1 )

yelp-marshal.c: yelp-marshal.h
	cd $(srcdir) && \
	( @GLIB_GENMARSHAL@ --prefix=yelp_marshal yelp-marshal.list --body > yelp-marshal2.tmp \
	&& mv yelp-marshal2.tmp yelp-marshal.c ) \
	|| ( rm -f yelp-marshal2.tmp && exit 1 )

yelp-marshal-main.c: yelp-marshal.c

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
	yelp-theme.c		yelp-theme.h			\
	yelp-toc-pager.c	yelp-toc-pager.h		\
	yelp-uri.c		yelp-uri.h			\
	yelp-utils.c		yelp-utils.h			\
	yelp-marshal-main.c	test-pager.c

test_pager_LDADD = @YELP_LIBS@ $(POPT_LIBS) $(Z_LIBS) $(BZ_LIBS)

test_uri_SOURCES =					\
	yelp-utils.c	yelp-utils.h			\
	test-uri.c

test_uri_LDADD = @YELP_LIBS@

yelp_SOURCES =							\
	$(gnome_yelp_idl_sources)				\
	yelp-base.c		yelp-base.h			\
	yelp-cache.c		yelp-cache.h			\
	yelp-db-pager.c		yelp-db-pager.h			\
	yelp-error.c		yelp-error.h			\
	yelp-html-gecko.c	yelp-html.h			\
	yelp-gecko-utils.cpp	yelp-gecko-utils.h		\
	yelp-io-channel.c	yelp-io-channel.h		\
	yelp-man-pager.c	yelp-man-pager.h		\
	yelp-man-parser.c	yelp-man-parser.h		\
	yelp-pager.c		yelp-pager.h			\
	yelp-toc-pager.c	yelp-toc-pager.h		\
	yelp-theme.c		yelp-theme.h			\
	yelp-window.c 		yelp-window.h			\
	yelp-uri.c	        yelp-uri.h			\
	yelp-utils.c		yelp-utils.h			\
	yelp-marshal-main.c	yelp-main.c

yelp_LDADD = @YELP_LIBS@ $(POPT_LIBS) $(Z_LIBS) $(BZ_LIBS) $(MOZILLA_COMPONENT_LIBS)

gnome_yelp_idl_sources =						\
	GNOME_Yelp-stubs.c						\
	GNOME_Yelp-skels.c						\
	GNOME_Yelp-common.c						\
	GNOME_Yelp.h

$(gnome_yelp_idl_sources): gnome_yelp_idl_stamp

gnome_yelp_idl_stamp: $(top_srcdir)/idl/GNOME_Yelp.idl $(ORBIT_IDL)
	$(ORBIT_IDL) $(YELP_IDL_INCLUDES) -I$(top_srcdir) $<
	touch $@

$(yelp_OBJECTS): yelp-marshal.h gnome_yelp_idl_stamp

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Yelp.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

CLEANFILES =								\
	$(gnome_yelp_idl_sources)					\
	gnome_yelp_idl_stamp						\
	GNOME_Yelp.server

EXTRA_DIST =								\
	yelp-marshal.list						\
	$(server_in_files)						\
	$(server_DATA)
