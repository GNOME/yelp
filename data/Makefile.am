SUBDIRS = icons ui

@INTLTOOL_XML_RULE@
@INTLTOOL_SCHEMAS_RULE@

schemasdir   = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA = yelp.schemas

tocdir   = $(datadir)/yelp
toc_DATA = man.xml toc.xml scrollkeeper.xml info.xml

cssdir   = $(datadir)/yelp
css_DATA = default.css

jsdir   = $(datadir)/yelp
js_DATA = yelp.js

EXTRA_DIST =					\
	yelp.schemas		yelp.schemas.in	\
	man.xml			man.xml.in	\
	toc.xml			toc.xml.in	\
	info.xml		info.xml.in	\
	scrollkeeper.xml	default.css	\
	yelp.js

install-data-local: $(schemas_DATA)
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)"; then \
	for p in $^ ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p >&1 > /dev/null; \
	done \
	fi
endif

CLEANFILES = $(schemas_DATA) toc.xml man.xml info.xml
