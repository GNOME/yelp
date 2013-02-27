bin_PROGRAMS = yelp

yelp_SOURCES =						\
	yelp-application.c	yelp-application.h	\
	yelp-window.c		yelp-window.h		\
	yelp.c

YELP_DEFINES =								\
	-DG_LOG_DOMAIN=\"Yelp\"						\
	-DPREFIX=\""$(prefix)"\"					\
	-DSYSCONFDIR=\""$(sysconfdir)"\"				\
	-DDATADIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\"					\
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
	$(POPT_LIBS)

yelp_LDFLAGS = $(AM_LDFLAGS)

install-exec-hook:
	rm -f $(DESTDIR)$(bindir)/gnome-help && \
	$(LN_S) yelp $(DESTDIR)$(bindir)/gnome-help
