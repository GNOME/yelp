INCLUDES =								\
	-I$(top_srcdir)							\
	$(YELP_CFLAGS)							\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = yelp

noinst_PROGRAMS = test-uri

test_uri_SOURCES = 							\
	test-uri.c							\
	yelp-uri.c		yelp-uri.h				\
	yelp-error.c		yelp-error.h				\
	yelp-util.c		yelp-util.h

test_uri_LDADD = 							\
	$(YELP_LIBS)

install-exec-local:
	rm -f $(bindir)/gnome-help
	ln -s yelp $(bindir)/gnome-help

yelp-marshal.h: yelp-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=yelp_marshal yelp-marshal.list --header > yelp-marshal1.tmp \
	&& mv yelp-marshal1.tmp yelp-marshal.h ) \
	|| ( rm -f yelp-marshal1.tmp && exit 1 )

yelp-marshal.c: yelp-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=yelp_marshal yelp-marshal.list --body > yelp-marshal2.tmp \
	&& mv yelp-marshal2.tmp yelp-marshal.c ) \
	|| ( rm -f yelp-marshal2.tmp && exit 1 )

yelp_SOURCES =								\
	$(gnome_yelp_idl_sources)					\
	yelp-marshal.c		yelp-marshal.h				\
	yelp-base.c		yelp-base.h				\
	yelp-db2html.c		yelp-db2html.h				\
	yelp-error.c		yelp-error.h				\
	yelp-history.c		yelp-history.h				\
	yelp-html.c		yelp-html.h				\
	yelp-index-model.c	yelp-index-model.h			\
	yelp-info.c		yelp-info.h				\
	yelp-main.c							\
	yelp-man.c		yelp-man.h				\
	yelp-section.c		yelp-section.h				\
	yelp-scrollkeeper.c	yelp-scrollkeeper.h			\
	yelp-util.c		yelp-util.h				\
	yelp-view-content.c	yelp-view-content.h			\
	yelp-view-index.c	yelp-view-index.h			\
	yelp-view-toc.c		yelp-view-toc.h				\
	yelp-window.c 		yelp-window.h				\
	yelp-uri.c	        yelp-uri.h

yelp_LDADD =								\
	$(YELP_LIBS)

gnome_yelp_idl_sources =						\
	GNOME_Yelp-stubs.c						\
	GNOME_Yelp-skels.c						\
	GNOME_Yelp.in							\
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
