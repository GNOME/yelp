SUBDIRS = dtd icons

#This has to be defined, because GSETTINGS_CHECK_RULE adds to it
MOSTLYCLEANFILES =

@INTLTOOL_XML_RULE@
@INTLTOOL_SCHEMAS_RULE@

gsettings_SCHEMAS = org.gnome.yelp.gschema.xml
@GSETTINGS_RULES@

tocdir   = $(datadir)/yelp
toc_DATA = man.xml toc.xml scrollkeeper.xml info.xml

cssdir   = $(datadir)/yelp
css_DATA = default.css

jsdir   = $(datadir)/yelp
js_DATA =				\
	jquery-1.4.2.min.js		\
	jquery-ui-1.8.custom.min.js	\
	yelp.js

EXTRA_DIST =					\
	man.xml			man.xml.in	\
	toc.xml			toc.xml.in	\
	info.xml		info.xml.in	\
	scrollkeeper.xml	default.css	\
	$(gsettings_SCHEMAS)	$(js_DATA)

CLEANFILES = toc.xml man.xml info.xml
