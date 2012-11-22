ALL_LINGUAS = @ALL_LINGUAS@

yelp.pot: yelp.xml.in
	itstool -o "$@" "$^"

yelp.xml: yelp.xml.in
	$(AM_V_GEN) ( \
	for lang in $(ALL_LINGUAS); do \
	  msgfmt -o "$$lang.mo" "$(top_srcdir)/po/$$lang.po" || exit 1; \
	done; \
	itstool -o "$@" -j "$^" $(foreach ll,$(ALL_LINGUAS),$(ll).mo); \
	rm $(foreach ll,$(ALL_LINGUAS),$(ll).mo))

xsldir=$(datadir)/yelp-xsl/xslt/common/domains

xsl_DATA = yelp.xml

EXTRA_DIST = yelp.xml.in yelp.xml yelp.pot
