SUBDIRS=dtd

DIRECTORIES=						\
	common						\
	html						\
	images						\
	lib

DOCBOOK_DIR=$(datadir)/sgml/docbook/yelp/docbook

install-data-local:													
	$(mkinstalldirs) $(DESTDIR)$(DOCBOOK_DIR);									\
	echo '-- Installing VERSION';											\
	$(INSTALL_DATA) VERSION $(DESTDIR)$(DOCBOOK_DIR);								\
	echo '-- Installing catalog';											\
	$(mkinstalldirs) $(DESTDIR)$(datadir)/yelp;
	$(INSTALL_DATA) catalog $(DESTDIR)$(datadir)/yelp;								\
	for DIR in $(DIRECTORIES); do											\
		echo '-- Installing '$$DIR;										\
		$(mkinstalldirs) $(DESTDIR)$(DOCBOOK_DIR)/$$DIR;							\
		(installfiles=`find $(srcdir)/$$DIR -name "*.xml" -o -name "*.xsl" -o -name "*.dtd" -o -name "*.png"`;	\
		 for i in $$installfiles; do 										\
			echo '-- Installing '$$i;									\
			$(INSTALL_DATA) $$i $(DESTDIR)$(DOCBOOK_DIR)/$$DIR;						\
		 done)													\
	done


dist-hook: 
	echo '-- Disting VERSION';											\
	cp VERSION $(distdir);												\
	echo '-- Disting catalog.in';											\
	cp catalog.in $(distdir);											\
	for DIR in $(DIRECTORIES); do											\
		echo '-- Disting '$$DIR;										\
		mkdir $(distdir)/$$DIR;											\
		(installfiles=`find $(srcdir)/$$DIR -name "*.xml" -o -name "*.xsl" -o -name "*.dtd" -o -name "*.png"`;	\
		 for i in $$installfiles; do 										\
			echo '-- Disting '$$i;										\
			cp $$i $(distdir)/$$DIR;									\
		 done)													\
	done
