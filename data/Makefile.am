SUBDIRS = icons ui

@INTLTOOL_XML_RULE@
@INTLTOOL_SCHEMAS_RULE@

schemasdir   = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA = yelp.schemas

tocdir   = $(datadir)/yelp
toc_DATA = toc.xml scrollkeeper.xml

cssdir   = $(datadir)/yelp
css_DATA = default.css

EXTRA_DIST =					\
	yelp.schemas		yelp.schemas.in	\
	toc.xml			toc.xml.in	\
	scrollkeeper.xml	default.css

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/yelp.schemas ; \
	fi
else
install-data-local:
endif

CLEANFILES = yelp.schemas toc.xml
