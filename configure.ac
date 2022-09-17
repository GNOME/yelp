AC_PREREQ([2.63])
# Reminder: add some release notes to data/yelp.appdata.xml.in on major releases
AC_INIT([Yelp],[42.2],[https://gitlab.gnome.org/GNOME/yelp/-/issues/],[yelp])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([src/yelp.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11.2 dist-xz no-dist-gzip subdir-objects tar-ustar])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

AX_IS_RELEASE([git-directory])
AX_CHECK_ENABLE_DEBUG([info], [YELP_DEBUG], [$ax_is_release])

AX_COMPILER_FLAGS([], [], [$ax_is_release])
# Check for programs
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_LN_S
GLIB_GSETTINGS
APPSTREAM_XML

# Release team wants WebKitGTK 4.1 for the GNOME 41 release.
# Shaun doesn't have a build environment with that yet.
# Get rid of this option when that stops being true.
AC_ARG_WITH([webkit2gtk-4-0],
        AS_HELP_STRING([--with-webkit2gtk-4-0],
        [Build against webkit2gtk-4.0 instead of webkit2gtk-4.1]),
        [webkitdep="webkit2gtk-4.0 >= 2.19.2";webextdep="webkit2gtk-web-extension-4.0 >= 2.7.2"],
        [webkitdep="webkit2gtk-4.1";webextdep="webkit2gtk-web-extension-4.1"])

# Library dependecies
LT_LIB_M
PKG_CHECK_MODULES(YELP,
[
	gio-2.0 >= 2.67.4
	gio-unix-2.0
	gtk+-unix-print-3.0
	gtk+-3.0 >= 3.13.3
	libhandy-1 >= 1.5.0
	libxml-2.0 >= 2.6.5
	libxslt >= 1.1.4
	libexslt >= 0.8.1
	sqlite3
        $webkitdep
	yelp-xsl >= 41.0
])
AC_SUBST([YELP_CFLAGS])
AC_SUBST([YELP_LIBS])


# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT

PKG_CHECK_MODULES([YELP_EXTENSION], [$webextdep])
AC_SUBST(YELP_EXTENSION_CFLAGS)
AC_SUBST(YELP_EXTENSION_LIBS)

# i18n stuff
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

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
XSL_TMPL="`$PKG_CONFIG --variable=xsltdir yelp-xsl`/common/tmpl.xsl"
AC_SUBST(XSL_TMPL)

XSL_JSDIR="`$PKG_CONFIG --variable=jsdir yelp-xsl`"
AC_SUBST(XSL_JSDIR)

XSL_GETTEXT="`$PKG_CONFIG --variable=gettext yelp-xsl`"
AC_SUBST(XSL_GETTEXT)

YELP_ICON_PATH="`$PKG_CONFIG --variable=icondir yelp-xsl`"
AC_SUBST(YELP_ICON_PATH)

GLIB_GENMARSHAL="`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`"
AC_SUBST(GLIB_GENMARSHAL)


AX_REQUIRE_DEFINED([GTK_DOC_CHECK])
GTK_DOC_CHECK([1.13],[--flavour no-tmpl])

AC_CONFIG_FILES([
Makefile
data/dtd/catalog
data/xslt/db2html.xsl
data/xslt/info2html.xsl
data/xslt/links2html.xsl
data/xslt/mal2html.xsl
data/xslt/man2html.xsl
data/xslt/yelp-common.xsl
po/Makefile.in
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
