SUBDIRS = src po

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES=yelp.desktop.in
DESKTOP_FILES=$(DESKTOP_IN_FILES:.desktop.in=.desktop)

Applicationsdir = $(datadir)/applications
Applications_DATA = $(DESKTOP_FILES)

yelpdatadir = $(datadir)/yelp
yelpdata_DATA = important_docs.xml


EXTRA_DIST = 				\
	intltool-extract.in  		\
	intltool-merge.in    		\
	intltool-update.in		\
	idl/GNOME_Yelp.idl		\
	important_docs.xml		\
	$(DESKTOP_IN_FILES)		\
	$(DESKTOP_FILES)

