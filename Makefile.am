SUBDIRS = src po

@INTLTOOL_DESKTOP_RULE@

DESKTOP_IN_FILES=yelp.desktop.in
DESKTOP_FILES=$(DESKTOP_IN_FILES:.desktop.in=.desktop)

Applicationsdir = $(datadir)/gnome/apps/Applications
Applications_DATA = $(DESKTOP_FILES)

EXTRA_DIST = 				\
	intltool-extract.in  		\
	intltool-merge.in    		\
	intltool-update.in		\
	idl/GNOME_Yelp.idl		\
	$(DESKTOP_IN_FILES)		\
	$(DESKTOP_FILES)

