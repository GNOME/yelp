INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(includedir)							\
	$(YELP_CFLAGS)							\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

bin_PROGRAMS = yelp

yelp_SOURCES =								\
	$(gnome_yelp_idl_sources)					\
	yelp-base.c		yelp-base.h				\
	yelp-history.c		yelp-history.h				\
	yelp-html.c		yelp-html.h				\
	yelp-main.c							\
	yelp-section.c		yelp-section.h				\
	yelp-scrollkeeper.c	yelp-scrollkeeper.h			\
	yelp-util.c		yelp-util.h				\
	yelp-view-content.c	yelp-view-content.h			\
	yelp-view-home.c	yelp-view-home.h			\
	yelp-view-index.c	yelp-view-index.h			\
	yelp-window.c 		yelp-window.h

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

$(yelp_OBJECTS): gnome_yelp_idl_stamp

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Yelp.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

CLEANFILES =								\
	$(gnome_yelp_idl_sources)					\
	gnome_yelp_idl_stamp						\
	GNOME_Yelp.server

EXTRA_DIST =								\
	$(server_in_files)						\
	$(server_DATA)

BUILT_SOURCES =								\
	$(gnome_yelp_idl_sources)
