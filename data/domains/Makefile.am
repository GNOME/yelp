ALL_LINGUAS = @ALL_LINGUAS@

yelp.pot: yelp.xml
	itstool -o "$@" "$^"

all: yelp.xml yelp.pot

yelp.xml: yelp.xml.in
	@echo "  GEN    $@"; \
	if [ ! -d .yelp ]; then mkdir .yelp; fi; \
	echo '<files>' > ".yelp.files"; \
	echo '<source href="yelp.xml.in"/>' >> ".yelp.files"; \
	for lang in $(ALL_LINGUAS); do \
	  msgfmt -o ".yelp/$$lang.mo" "$(top_srcdir)/po/$$lang.po" || exit 1; \
	  (cd .yelp && itstool -o "$$lang.xml" -m "$$lang.mo" "../yelp.xml.in") || exit 1; \
	  rm ".yelp/$$lang.mo" || exit 1; \
	  echo '<file href="'".yelp/$$lang.xml"'"/>' >> ".yelp.files"; \
	done; \
	echo '</files>' >> ".yelp.files"; \
	xsltproc -o "$@" "$(srcdir)/msgs.xsl" ".yelp.files" || exit 1; \
	for lang in $(ALL_LINGUAS); do rm ".yelp/$$lang.xml"; done; \
	rmdir .yelp; \
	rm .yelp.files

xsldir=$(datadir)/yelp-xsl/xslt/common/domains

xsl_DATA = yelp.xml

EXTRA_DIST = msgs.xsl yelp.xml.in yelp.xml yelp.pot
