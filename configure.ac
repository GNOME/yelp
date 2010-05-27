AC_PREREQ([2.63])
AC_INIT([Yelp],[2.31.3],[http://bugzilla.gnome.org/enter_bug.cgi?product=yelp],[yelp])

AC_CONFIG_SRCDIR([src/yelp.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_LN_S

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT

AC_PATH_PROGS(SED, gsed sed)

AM_PATH_GLIB_2_0
GLIB_GSETTINGS

# Library dependecies
PKG_CHECK_MODULES(YELP,
[
	gio-2.0 >= 2.25.3
	gio-unix-2.0
	gtk+-unix-print-2.0
	gtk+-2.0 >= 2.16.0
	libxml-2.0 >= 2.6.5
	libxslt >= 1.1.4
	libexslt >= 0.8.1
	dbus-glib-1
	webkit-1.0 >= 1.1.15
	yelp-xsl
])
AC_SUBST([YELP_CFLAGS])
AC_SUBST([YELP_LIBS])


GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

MORE_WARN_FLAGS=

if test "x$enable_debug" = "xyes"; then
	AC_DEFINE([YELP_DEBUG],[1],[Enable Yelp debugging])
	AM_CFLAGS="$AM_CFLAGS -pedantic -ansi"
fi

dnl NOTE : it appears that some systems do not like it when you use line
dnl        continuation charactes to split CFLAGS into readable chunks
if test "x$GCC" = "xyes"; then
   AM_CFLAGS="$AM_CFLAGS -Wall -Wno-uninitialized -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wno-sign-compare"
fi

# i18n stuff
IT_PROG_INTLTOOL([0.41.0])

GETTEXT_PACKAGE=yelp
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext translation domain])

AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR([GNOMELOCALEDIR])

LIBLZMA_REQUIRED=4.9
AC_ARG_ENABLE(lzma,
	AS_HELP_STRING([--enable-lzma],[Enable lzma decoder support [[default=yes]]]),
	[enable_lzma=$enableval],
	[enable_lzma="auto"])
if test x"$enable_lzma" = "xauto"; then
	PKG_CHECK_MODULES(YELP_LZMA,
			  liblzma >= $LIBLZMA_REQUIRED,
			  enable_lzma=yes,
			  enable_lzma=no)
elif test x"$enable_lzma" = "xyes"; then
	PKG_CHECK_MODULES(YELP_LZMA, liblzma >= $LIBLZMA_REQUIRED)
fi
if test x"$enable_lzma" = "xyes"; then
	YELP_CFLAGS="$YELP_LZMA_CFLAGS $YELP_CFLAGS";
	YELP_LIBS="$YELP_LZMA_LIBS $YELP_LIBS";
	AC_DEFINE(ENABLE_LZMA, 1, [Enable lzma decoder support])
fi
AM_CONDITIONAL([ENABLE_LZMA],[test x"$enable_lzma" != "xno"])

AC_ARG_ENABLE(bz2,
	AS_HELP_STRING([--enable-bz2],[Enable bz2 decoder support [[default=yes]]]),
	[enable_bz2=$enableval],
	[enable_bz2="auto"])
if test x"$enable_bz2" = "xauto"; then
	AC_CHECK_LIB(bz2,
		     BZ2_bzread,
		     enable_bz2=yes,
		     enable_bz2=no)
elif test x"$enable_bz2" = "xyes"; then
	AC_CHECK_LIB(bz2, BZ2_bzread)
fi
if test x"$enable_bz2" = "xyes"; then
	YELP_LIBS="-lbz2 $YELP_LIBS";
	AC_DEFINE(ENABLE_BZ2, 1, [Enable bz2 decoder support])
fi
AM_CONDITIONAL([ENABLE_BZ2],[test x"$enable_bz2" != "xno"])

AC_SUBST([YELP_CFLAGS])
AC_SUBST([YELP_LIBS])

DATADIR=`(
	case $prefix in
		NONE) prefix=$ac_default_prefix ;;
		*) ;;
	esac
	eval echo $(eval echo $datadir)/yelp
)`
AC_SUBST([DATADIR])


XSL_PATH="`$PKG_CONFIG --variable=xsltdir yelp-xsl`"
XSL_DB_TITLE="$XSLT_PATH""/docbook/common/db-title.xsl"
AC_SUBST(XSL_DB_TITLE)

XSL_DB2XHTML="`$PKG_CONFIG --variable=db2xhtml yelp-xsl`"
AC_SUBST(XSL_DB2XHTML)

XSL_MAL2XHTML="`$PKG_CONFIG --variable=mal2xhtml yelp-xsl`"
AC_SUBST(XSL_MAL2XHTML)

XSL_THEME_COLORS="`$PKG_CONFIG --variable=xsltdir yelp-xsl`/theme/theme-colors.xsl"
AC_SUBST(XSL_THEME_COLORS)
XSL_THEME_ICONS="`$PKG_CONFIG --variable=xsltdir yelp-xsl`/theme/theme-icons.xsl"
AC_SUBST(XSL_THEME_ICONS)
XSL_THEME_HTML="`$PKG_CONFIG --variable=xsltdir yelp-xsl`/theme/theme-html.xsl"
AC_SUBST(XSL_THEME_HTML)

XSL_GETTEXT="`$PKG_CONFIG --variable=gettext yelp-xsl`"
AC_SUBST(XSL_GETTEXT)

YELP_ICON_PATH="`$PKG_CONFIG --variable=icondir yelp-xsl`"
AC_SUBST(YELP_ICON_PATH)

GLIB_GENMARSHAL="`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`"
AC_SUBST(GLIB_GENMARSHAL)

#
# If Pango included the shared library dependencies from X11 in
# the pkg-config output, then we use that (to avoid duplicates).
# but if they were omitted to avoid binary compatibility problems
# then we need to repeat the checks.
#
if $PKG_CONFIG --exists pangoxft ; then
  PANGO_PACKAGES="pangox pangoxft"
else
  PANGO_PACKAGES="pangox"
fi

x_libs="`$PKG_CONFIG --libs $PANGO_PACKAGES`"
case x_libs in
  *-lX11*) pango_omitted_x_deps=no ;;
  *)       pango_omitted_x_deps=yes ;;
esac

if test $pango_omitted_x_deps = yes ; then
  AC_PATH_XTRA

  if test x$no_x = xyes ; then
    AC_MSG_ERROR([X development libraries not found])
  else
    X_LIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
  fi
fi

AC_SUBST(X_LIBS)


# Set flags
AM_CPPFLAGS="$AM_CPPFLAGS $DEPRECATION_FLAGS"
AM_CFLAGS="$AM_CFLAGS $WARN_CFLAGS $MORE_WARN_FLAGS"


AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

GTK_DOC_CHECK([1.13],[--flavour no-tmpl])

AC_CONFIG_FILES([
Makefile
docs/Makefile
docs/libyelp/Makefile
docs/libyelp/version.xml
libyelp/Makefile
src/Makefile
stylesheets/Makefile
stylesheets/db2html.xsl
stylesheets/db-title.xsl
stylesheets/info2html.xsl
stylesheets/mal2html.xsl
stylesheets/toc2html.xsl
stylesheets/yelp-common.xsl
data/Makefile
data/icons/Makefile
data/icons/hicolor/Makefile
data/icons/hicolor/16x16/Makefile
data/icons/hicolor/16x16/status/Makefile
po/Makefile.in
tests/Makefile
yelp.desktop.in
])

AC_OUTPUT

dnl ==========================================================================
echo "
yelp-$VERSION:

	prefix:                   ${prefix}
	source code location:	  ${srcdir}
	compiler:		  ${CC}

	Documentation:            ${enable_gtk_doc}
	Debug enabled:            ${enable_debug}
	Search backend:           ${search_backend}
	Enable BZ2:               ${enable_bz2}
	Enable LZMA:              ${enable_lzma}

"
