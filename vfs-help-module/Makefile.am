INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(includedir)					\
	$(YELP_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Yelp\"

modules_LTLIBRARIES=libvfs-help.la

module_flags = -export_dynamic -avoid-version
modulesdir   = $(libdir)/gnome-vfs-2.0/modules

libvfs_help_la_SOURCES = 				\
	help-method.c		help-method.h		

libvfs_help_la_LDFLAGS = $(module_flags)
libvfs_help_la_LIBADD  = $(YELP_LIBS)

modulesconfdir=$(sysconfdir)/gnome-vfs-2.0/modules
modulesconf_DATA = help-methods.conf

EXTRA_DIST=help-methods.conf
