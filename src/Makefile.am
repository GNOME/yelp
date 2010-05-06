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
	$(YELP_SEARCH_CFLAGS)			\
	$(AM_CFLAGS)				\
	$(YELP_DEFINES)

yelp_LDADD =					\
	$(top_builddir)/libyelp/libyelp.la	\
	$(YELP_LIBS)				\
	$(YELP_SEARCH_LIBS)			\
	$(POPT_LIBS)				\
	$(Z_LIBS)				\
	$(BZ_LIBS)				\
	$(LZMADEC_LIBS)				\
	$(X_LIBS)

yelp_LDFLAGS = $(AM_LDFLAGS)

if WITH_SMCLIENT
yelp_SOURCES += \
	eggdesktopfile.c \
	eggdesktopfile.h \
	eggsmclient.c \
	eggsmclient.h \
	eggsmclient-private.h \
	$(NULL)
yelp_CFLAGS += $(SMCLIENT_CFLAGS)
yelp_LDADD += $(SMCLIENT_LIBS)

if WITH_SMCLIENT_XSMP
yelp_SOURCES += eggsmclient-xsmp.c
yelp_CFLAGS += -DEGG_SM_CLIENT_BACKEND_XSMP
endif
if WITH_SMCLIENT_WIN32
yelp_SOURCES += eggsmclient-win32.c
endif
if WITH_SMCLIENT_QUARTZ
yelp_SOURCES += eggsmclient-osx.c
endif
endif

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
