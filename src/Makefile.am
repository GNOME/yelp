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
	yelp-main.c					\
	yelp-base.c		yelp-base.h		\
	yelp-window.c 		yelp-window.h		\
	yelp-toc.c    		yelp-toc.h		\
	yelp-doc-view.c		yelp-doc-view.h		\
	yelp-index.c		yelp-index.h		\
	yelp-history.c		yelp-history.h		\
	yelp-section.c		yelp-section.h		\
	yelp-scrollkeeper.c	yelp-scrollkeeper.h	\
	yelp-util.c		yelp-util.h

yelp_LDADD =						\
	$(YELP_LIBS)
