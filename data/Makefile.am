SUBDIRS = icons ui

@INTLTOOL_XML_RULE@
@INTLTOOL_SCHEMAS_RULE@

tocdir   = $(datadir)/yelp
toc_DATA = man.xml toc.xml scrollkeeper.xml info.xml

cssdir   = $(datadir)/yelp
css_DATA = default.css

jsdir   = $(datadir)/yelp
js_DATA = yelp.js

EXTRA_DIST =					\
	man.xml			man.xml.in	\
	toc.xml			toc.xml.in	\
	info.xml		info.xml.in	\
	scrollkeeper.xml	default.css	\
	yelp.js

CLEANFILES = toc.xml man.xml info.xml

