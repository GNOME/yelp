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
	main.c						\
	yelp-base.c		yelp-base.h		\
	yelp-window.c 		yelp-window.h		\
	yelp-toc.c    		yelp-toc.h		\
	yelp-view.c		yelp-view.h		\
	yelp-index.c		yelp-index.h		\
	yelp-history.c		yelp-history.h		\
	yelp-section.c		yelp-section.h		\
	yelp-keyword-db.c	yelp-keyword-db.h	\
	metadata-parser.c	metadata-parser.h	\
	scrollkeeper-parser.c	scrollkeeper-parser.h

yelp_LDADD =						\
	$(YELP_LIBS)
