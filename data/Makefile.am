SUBDIRS = icons ui

@INTLTOOL_XML_RULE@
@INTLTOOL_SCHEMAS_RULE@

schemasdir   = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA = yelp.schemas

tocdir   = $(datadir)/yelp
toc_DATA = man.xml toc.xml scrollkeeper.xml

cssdir   = $(datadir)/yelp
css_DATA = default.css

jsdir   = $(datadir)/yelp
js_DATA = yelp.js

EXTRA_DIST =					\
	yelp.schemas		yelp.schemas.in	\
	man.xml			man.xml.in	\
	toc.xml			toc.xml.in	\
	scrollkeeper.xml	default.css	\
	yelp.js

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/yelp.schemas ; \
	fi
else
install-data-local:
endif

CLEANFILES = yelp.schemas toc.xml man.xml
