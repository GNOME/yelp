DIRECTORIES=									\
	ent

DTD_DIR=$(datadir)/sgml/docbook/yelp/docbook/dtd

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(DTD_DIR);					\
	echo '-- Installing catalog';						\
	$(INSTALL_DATA) catalog $(DESTDIR)$(DTD_DIR);				\
	(installfiles=`find $(srcdir) -name "*.dtd" -o -name "*.mod"`;		\
	 for i in $$installfiles; do						\
		echo '-- Installing '$$i;					\
		$(INSTALL_DATA) $$i $(DESTDIR)$(DTD_DIR);			\
	done);									\
	for DIR in $(DIRECTORIES); do						\
		echo '-- Installing '$$DIR;					\
		$(mkinstalldirs) $(DESTDIR)$(DTD_DIR)/$$DIR;			\
		(installfiles=`find $(srcdir)/$$DIR -name "*.ent"`;		\
		 for i in $$installfiles; do 					\
			echo '-- Installing '$$i;				\
			$(INSTALL_DATA) $$i $(DESTDIR)$(DTD_DIR)/$$DIR;		\
		 done)								\
	done

dist-hook:
	echo '-- Disting catalog';						\
	cp catalog $(distdir);							\
	(installfiles=`find $(srcdir) -name "*.dtd" -o -name "*.mod"`;		\
	 for i in $$installfiles; do						\
	 	echo '-- Disting '$$i;						\
		cp $$i $(distdir);						\
	 done);									\
	for DIR in $(DIRECTORIES); do						\
		echo '-- Disting '$$DIR;					\
		mkdir $(distdir)/$$DIR;						\
		(installfiles=`find $(srcdir)/$$DIR -name "*.ent"`;		\
		 for i in $$installfiles; do 					\
			echo '-- Disting '$$i;					\
			cp $$i $(distdir)/$$DIR;				\
		 done)								\
	done
