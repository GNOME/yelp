SUBDIRS = man2html info2html

INCLUDES =								\
	-I$(top_srcdir)							\
	@YELP_CFLAGS@							\
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

libexec_PROGRAMS = yelp-db2html
bin_PROGRAMS = yelp yelp-pregenerate

noinst_PROGRAMS = test-uri test-reader test-pager
test_uri_SOURCES = 							\
	test-uri.c							\
	yelp-uri.c		yelp-uri.h

test_uri_LDADD = 							\
	@YELP_LIBS@

test_reader_SOURCES = 							\
	test-reader.c							\
	yelp-uri.c		yelp-uri.h				\
	yelp-cache.c		yelp-cache.h				\
	yelp-reader.c		yelp-reader.h				\
	yelp-error.c		yelp-error.h				\
	yelp-util.c		yelp-util.h				\
	yelp-marshal-main.c

test_reader_LDADD = 							\
	@YELP_LIBS@

test_pager_SOURCES =							\
	test-pager.c							\
	yelp-pager.c		yelp-pager.h				\
	yelp-db-pager.c		yelp-db-pager.h				\
	yelp-uri.c		yelp-uri.h				\
	yelp-marshal-main.c

test_pager_LDADD =							\
	@DB2HTML_LIBS@ @YELP_LIBS@ -lexslt

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

yelp_pregenerate_SOURCES =						\
	yelp-pregenerate.c      yelp-pregenerate.h

yelp_db2html_SOURCES =							\
	yelp-db2html.c

yelp_db2html_LDADD =							\
	@DB2HTML_LIBS@ -lexslt

yelp_pregenerate_LDADD =						\
	@YELP_LIBS@ -lexslt

yelp_SOURCES =								\
	$(gnome_yelp_idl_sources)					\
	yelp-base.c		yelp-base.h				\
	yelp-cache.c		yelp-cache.h				\
	yelp-db-pager.c		yelp-db-pager.h				\
	yelp-error.c		yelp-error.h				\
	yelp-history.c		yelp-history.h				\
	yelp-html-gtkhtml2.c	yelp-html.h				\
	yelp-index-model.c	yelp-index-model.h			\
	yelp-info.c		yelp-info.h				\
	yelp-main.c							\
	yelp-man.c		yelp-man.h				\
	yelp-marshal-main.c						\
	yelp-reader.c		yelp-reader.h				\
	yelp-section.c		yelp-section.h				\
	yelp-scrollkeeper.c	yelp-scrollkeeper.h			\
	yelp-pager.c		yelp-pager.h				\
	yelp-util.c		yelp-util.h				\
	yelp-window.c 		yelp-window.h				\
	yelp-uri.c	        yelp-uri.h

yelp_LDADD =								\
	@YELP_LIBS@							\
	$(POPT_LIBS)

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
