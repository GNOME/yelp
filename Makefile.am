ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = libyelp src po data tests docs

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files=yelp.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

EXTRA_DIST = 						\
	autogen.sh					\
	MAINTAINERS					\
	intltool-extract.in  				\
	intltool-merge.in    				\
	intltool-update.in				\
	$(desktop_in_files)

DISTCLEANFILES = 					\
	intltool-extract 				\
	intltool-merge 					\
	intltool-update

CLEANFILES = $(desktop_DATA)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
