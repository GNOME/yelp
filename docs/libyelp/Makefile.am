AUTOMAKE_OPTIONS = 1.6

DOC_MODULE = libyelp

DOC_MAIN_SGML_FILE = libyelp-docs.xml

SCAN_OPTIONS = --deprecated-guards="G_DISABLE_DEPRECATED" --rebuild-types

SCANGOBJ_OPTIONS = --type-init-func="g_type_init();"

DOC_SOURCE_DIR = $(top_srcdir)/libyelp

GTKDOC_CFLAGS =						\
	$(YELP_CFLAGS)					\
	-I$(top_srcdir)/libyelp

GTKDOC_LIBS =					\
	$(YELP_LIBS)				\
	$(top_builddir)/libyelp/libyelp.la

HFILE_GLOB = $(top_srcdir)/libyelp/*.h
CFILE_GLOB = $(top_srcdir)/libyelp/*.c
IGNORE_HFILES = yelp-debug.h yelp-types.h

MKDB_OPTIONS = --output-format=xml --sgml-mode --name-space=yelp --ignore-files='yelp-debug.c yelp-debug.h yelp-types.c yelp-types.h'

content_files = version.xml

extra_files = version.xml.in

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST += version.xml.in

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
#MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-mkhtml
#MKHTML_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
#FIXXREF_OPTIONS=

# Extra header to include when scanning, which are not under DOC_SOURCE_DIR
# e.g. EXTRA_HFILES=$(top_srcdir}/contrib/extra.h
#EXTRA_HFILES=


# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
#HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
#content_files=

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
#expand_content_files=

# This includes the standard gtk-doc make rules, copied by gtkdocize.

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
#EXTRA_DIST +=

# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
#DISTCLEANFILES +=

# Comment this out if you want your docs-status tested during 'make check'
#if ENABLE_GTK_DOC
#TESTS_ENVIRONMENT = cd $(srcsrc) &&
#TESTS = $(GTKDOC_CHECK)
#endif
