bin_PROGRAMS = yelp

yelp_SOURCES =						\
	yelp-application.c	yelp-application.h	\
	yelp-window.c		yelp-window.h		\
	yelp.c

#	yelp-toc.h		yelp-toc.c			\
#	yelp-db-print.c		yelp-db-print.h			\
#	yelp-man-parser.c 	yelp-man-parser.h		\
#	yelp-man.c 		yelp-man.h			\
#	yelp-search.c		yelp-search.h			\
#	yelp-search-parser.c 	yelp-search-parser.h


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
	-DGDU_ICON_PATH=\"$(GDU_ICON_PATH)\"

yelp_CFLAGS =					\
	-I$(top_srcdir)/libyelp			\
	$(YELP_CFLAGS)				\
	$(AM_CFLAGS)				\
	$(YELP_DEFINES)

yelp_LDADD =					\
	$(top_builddir)/libyelp/libyelp.la	\
	$(YELP_LIBS)				\
	$(POPT_LIBS)				\
	$(X_LIBS)

yelp_LDFLAGS = $(AM_LDFLAGS)


BUILT_SOURCES = yelp-dbus.h

noinst_HEADERS = $(BUILT_SOURCES)

yelp-dbus.h: yelp-dbus.xml
	dbus-binding-tool --prefix=yelp --mode=glib-server $<> $@

MAINTAINERCLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = yelp-dbus.xml

install-exec-hook:
	rm -f $(DESTDIR)$(bindir)/gnome-help && \
	$(LN_S) yelp $(DESTDIR)$(bindir)/gnome-help

distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi
