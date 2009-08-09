SUBDIRS = src stylesheets po data tests

ACLOCAL_AMFLAGS = -I m4

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files=yelp.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

yelpdatadir = $(datadir)/yelp
yelpdata_DATA = important_docs.xml

EXTRA_DIST = 						\
	autogen.sh					\
	MAINTAINERS					\
	intltool-extract.in  				\
	intltool-merge.in    				\
	intltool-update.in				\
	important_docs.xml				\
	$(desktop_in_files)

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update

CLEANFILES = $(desktop_DATA)

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper
