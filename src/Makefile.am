INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(includedir)					\
	$(YELP_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Yelp\"				\
	-DPREFIX=\""$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"


bin_PROGRAMS = yelp

yelp_SOURCES =						\
	yelp-base.c		yelp-base.h		\
	yelp-history.c		yelp-history.h		\
	yelp-html.c		yelp-html.h		\
	yelp-main.c					\
	yelp-section.c		yelp-section.h		\
	yelp-scrollkeeper.c	yelp-scrollkeeper.h	\
	yelp-util.c		yelp-util.h		\
	yelp-view-content.c	yelp-view-content.h	\
	yelp-view-home.c	yelp-view-home.h	\
	yelp-view-index.c	yelp-view-index.h	\
	yelp-window.c 		yelp-window.h

yelp_LDADD =						\
	$(YELP_LIBS)
