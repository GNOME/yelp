SUBDIRS = domains dtd icons mathjax xslt

#This has to be defined, because GSETTINGS_CHECK_RULE adds to it
MOSTLYCLEANFILES =

@INTLTOOL_SCHEMAS_RULE@

gsettings_SCHEMAS = org.gnome.yelp.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = $(gsettings_SCHEMAS)
