## Process this file with automake to produce Makefile.in.

INCLUDES = $(LIBGNOME_CFLAGS)			\
	   -I$(top_srcdir)

libexec_PROGRAMS = gnome2-man2html

gnome2_man2html_SOURCES = gnome-man2html.c

gnome2_man2html_LDADD = \
	$(top_builddir)/libgnome/libgnome-2.la		\
	$(LIBGNOME_LIBS) $(Z_LIBS) $(BZ_LIBS)

#CFLAGS += -Wall

