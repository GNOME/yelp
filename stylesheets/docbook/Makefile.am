SUBDIRS=dtd

DOCBOOK_DIR=$(datadir)/sgml/docbook/yelp/docbook

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(DOCBOOK_DIR);			\
	echo '-- Installing VERSION';					\
	$(INSTALL_DATA) VERSION $(DESTDIR)$(DOCBOOK_DIR);		\
	echo '-- Installing catalog';					\
	$(mkinstalldirs) $(DESTDIR)$(datadir)/yelp;			\
	$(INSTALL_DATA) catalog $(DESTDIR)$(datadir)/yelp;

dist-hook: 
	echo '-- Disting VERSION';					\
	cp VERSION $(distdir);						\
	echo '-- Disting catalog.in';					\
	cp catalog.in $(distdir);
