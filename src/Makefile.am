INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(includedir)					\
	$(YELP_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Yelp\"

bin_PROGRAMS = yelp ghelp-test

yelp_SOURCES =						\
	main.c						\
	yelp-base.c		yelp-base.h		\
	yelp-window.c 		yelp-window.h		\
	yelp-toc.c    		yelp-toc.h		\
	yelp-view.c		yelp-view.h		\
	yelp-index.c		yelp-index.h		\
	yelp-history.c		yelp-history.h		\
	yelp-book.c		yelp-book.h		\
	yelp-keyword-db.c	yelp-keyword-db.h	\
	metadata-parser.c	metadata-parser.h	\
	devhelp-parser.c	devhelp-parser.h	\
	scrollkeeper-parser.c	scrollkeeper-parser.h	\
	ghelp-uri.c		ghelp-uri.h

yelp_LDADD =						\
	$(YELP_LIBS)

ghelp_test_SOURCES =					\
	ghelp-main.c					\
	ghelp-uri.c		ghelp-uri.h

ghelp_test_LDADD =					\
	$(YELP_LIBS)
