yelp_search_provider_CFLAGS = \
	-I$(top_srcdir)/libyelp \
	$(YELP_CFLAGS) \
	$(NULL)

libexec_PROGRAMS = yelp-search-provider

dbus_shell_search_provider_built_sources =	\
	yelp-shell-search-provider-generated.c	\
	yelp-shell-search-provider-generated.h

$(dbus_shell_search_provider_built_sources) : Makefile.am $(srcdir)/org.gnome.ShellSearchProvider2.xml
	$(AM_V_GEN)gdbus-codegen					\
		--interface-prefix org.gnome.				\
		--c-namespace Yelp					\
		--generate-c-code yelp-shell-search-provider-generated	\
		$(srcdir)/org.gnome.ShellSearchProvider2.xml		\
		$(NULL)

yelp_search_provider_SOURCES = \
	$(dbus_shell_search_provider_built_sources) \
	yelp-search-provider.c \
	$(NULL)

yelp_search_provider_LDADD = $(YELP_LIBS) ../libyelp/libyelp.la

searchproviderdir = $(datadir)/gnome-shell/search-providers
searchprovider_DATA = yelp-search-provider.ini

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = org.gnome.Yelp.SearchProvider.service.in

org.gnome.Yelp.SearchProvider.service: org.gnome.Yelp.SearchProvider.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

BUILT_SOURCES = $(dbus_shell_search_provider_built_sources)

EXTRA_DIST = \
	$(service_in_files) \
	org.gnome.ShellSearchProvider2.xml \
	yelp-search-provider.ini

CLEANFILES = org.gnome.Yelp.SearchProvider.service
