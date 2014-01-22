SUBDIRS = domains dtd icons mathjax xslt

#This has to be defined, because GSETTINGS_CHECK_RULE adds to it
MOSTLYCLEANFILES =

@INTLTOOL_SCHEMAS_RULE@

appdatadir = $(datadir)/appdata
appdata_in_files = yelp.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

gsettings_SCHEMAS = org.gnome.yelp.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(appdata_in_files) \
	$(gsettings_SCHEMAS)

CLEANFILES = \
	$(appdata_DATA)
