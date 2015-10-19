ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = libyelp libyelp/web-extension src po data tests docs

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files=yelp.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

EXTRA_DIST = 						\
	autogen.sh					\
	$(desktop_in_files)

CLEANFILES = $(desktop_DATA)

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-bz2 \
	--enable-gtk-doc \
	--enable-lzma
