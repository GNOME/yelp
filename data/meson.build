yelp_datadir = join_paths(datadir, 'yelp')

subdir('domains')
subdir('dtd')
subdir('xslt')

# we build it by default, but ignore the result - this is to ensure
# that schema file is valid
gnome.compile_schemas(build_by_default: true,
                      #depend_files: ['org.gnome.yelp.gschema.xml'],
)

install_data('org.gnome.yelp.gschema.xml',
             install_dir : join_paths(datadir, 'glib-2.0/schemas'))

install_subdir('dtd',
               install_dir: yelp_datadir,
               exclude_files: [
                 'meson.build',
                 'catalog.in',
               ])

install_subdir('mathjax',
               install_dir: yelp_datadir,
               exclude_files: [
                 'LICENSE',
                 'README',
                 'jax/element/mml/optable/Makefile.am',
                 'jax/input/MathML/entities/Makefile.am',
                 'jax/output/Makefile.am',
                 'jax/output/HTML-CSS/fonts/TeX/SansSerif/Bold/Makefile.am',
                 'jax/output/HTML-CSS/fonts/TeX/SansSerif/Italic/Makefile.am',
                 'jax/output/HTML-CSS/fonts/TeX/SansSerif/Regular/Makefile.am',
               ],
               exclude_directories: [
                 'unpacked',
               ])

install_subdir('yelp-icons',
               install_dir: yelp_datadir,
               exclude_files: [
                 'yelp-watermark-blockquote.xcf',
               ])

install_subdir('icons',
               install_dir: datadir)

i18n.merge_file('appdata',
                input: 'yelp.appdata.xml.in',
                output: 'yelp.appdata.xml',
                install: true,
                install_dir: join_paths(datadir, 'metainfo'),
                po_dir: join_paths(meson.source_root(), 'po'),
                type: 'xml')
