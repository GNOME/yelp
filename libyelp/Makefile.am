lib_LTLIBRARIES = libyelp.la

libyelp_la_SOURCES =				\
	yelp-debug.c				\
	yelp-document.c				\
	yelp-location-entry.c			\
	yelp-settings.c				\
	yelp-simple-document.c			\
	yelp-uri.c				\
	yelp-view.c

libyelp_la_CFLAGS =				\
	$(YELP_CFLAGS)				\
	-DGDU_ICON_PATH=\"$(GDU_ICON_PATH)\"

libyelp_la_LIBADD =				\
	$(YELP_LIBS)
