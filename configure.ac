AC_PREREQ([2.63])
AC_INIT([Yelp],[3.10.2],[http://bugzilla.gnome.org/enter_bug.cgi?product=yelp],[yelp])

AC_CONFIG_SRCDIR([src/yelp.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-bzip2 no-dist-gzip tar-ustar])
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
	gio-2.0 >= 2.25.11
	gio-unix-2.0
	gtk+-unix-print-3.0
	gtk+-3.0 >= 2.91.8
	libxml-2.0 >= 2.6.5
	libxslt >= 1.1.4
	libexslt >= 0.8.1
	sqlite3
	webkitgtk-3.0 >= 1.3.10
	yelp-xsl >= 3.1.2
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

ALL_LINGUAS="`grep -v '^#' "$srcdir/po/LINGUAS" | tr '\n' ' '`"
AC_SUBST(ALL_LINGUAS)

GETTEXT_PACKAGE=yelp
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext translation domain])

AC_ARG_VAR([ITSTOOL], [Path to the `itstool` command])
AC_CHECK_PROG([ITSTOOL], [itstool], [itstool])
if test x"$ITSTOOL" = x; then
  AC_MSG_ERROR([itstool is required to build the translation XML files])
fi
ITSTOOL_VERSION=`itstool --version | cut -d' ' -f2 | awk -F. '{print $ 1 * 10000 + $ 2 * 100 + $ 3; }'`
if test "$ITSTOOL_VERSION" -lt 10200; then
  AC_MSG_ERROR([itstool >= 1.2.0 is required to build the translation XML files])
fi

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

XSL_DB2XHTML="`$PKG_CONFIG --variable=db2xhtml yelp-xsl`"
AC_SUBST(XSL_DB2XHTML)

XSL_MAL2XHTML="`$PKG_CONFIG --variable=mal2xhtml yelp-xsl`"
AC_SUBST(XSL_MAL2XHTML)

XSL_COLOR="`$PKG_CONFIG --variable=xsltdir yelp-xsl`/common/color.xsl"
AC_SUBST(XSL_COLOR)
XSL_ICONS="`$PKG_CONFIG --variable=xsltdir yelp-xsl`/common/icons.xsl"
AC_SUBST(XSL_ICONS)
XSL_HTML="`$PKG_CONFIG --variable=xsltdir yelp-xsl`/common/html.xsl"
AC_SUBST(XSL_HTML)
XSL_JSDIR="`$PKG_CONFIG --variable=jsdir yelp-xsl`"
AC_SUBST(XSL_JSDIR)

XSL_GETTEXT="`$PKG_CONFIG --variable=gettext yelp-xsl`"
AC_SUBST(XSL_GETTEXT)

YELP_ICON_PATH="`$PKG_CONFIG --variable=icondir yelp-xsl`"
AC_SUBST(YELP_ICON_PATH)

GLIB_GENMARSHAL="`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`"
AC_SUBST(GLIB_GENMARSHAL)


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
data/Makefile
data/domains/Makefile
data/dtd/Makefile
data/dtd/catalog
data/icons/Makefile
data/icons/hicolor/Makefile
data/icons/hicolor/16x16/Makefile
data/icons/hicolor/16x16/status/Makefile
data/icons/hicolor/scalable/Makefile
data/icons/hicolor/scalable/actions/Makefile
data/icons/hicolor/scalable/status/Makefile
data/mathjax/Makefile
data/mathjax/config/Makefile
data/mathjax/extensions/HTML-CSS/Makefile
data/mathjax/extensions/Makefile
data/mathjax/fonts/HTML-CSS/TeX/woff/Makefile
data/mathjax/jax/Makefile
data/mathjax/jax/element/Makefile
data/mathjax/jax/element/mml/Makefile
data/mathjax/jax/element/mml/optable/Makefile
data/mathjax/jax/input/Makefile
data/mathjax/jax/input/MathML/Makefile
data/mathjax/jax/input/MathML/entities/Makefile
data/mathjax/jax/output/HTML-CSS/Makefile
data/mathjax/jax/output/HTML-CSS/autoload/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/AMS/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Bold/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Caligraphic/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Bold/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Fraktur/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Bold/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/BoldItalic/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Italic/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Greek/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Bold/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Italic/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Main/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Math/BoldItalic/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Math/Italic/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Script/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size1/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size2/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size3/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Size4/Regular/Makefile
data/mathjax/jax/output/HTML-CSS/fonts/TeX/Typewriter/Regular/Makefile
data/mathjax/jax/output/Makefile
data/mathjax/jax/output/NativeMML/Makefile
data/mathjax/unpacked/Makefile
data/xslt/Makefile
data/xslt/db2html.xsl
data/xslt/info2html.xsl
data/xslt/mal2html.xsl
data/xslt/man2html.xsl
data/xslt/yelp-common.xsl
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
	Enable BZ2:               ${enable_bz2}
	Enable LZMA:              ${enable_lzma}

"
