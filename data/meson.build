yelp_datadir = join_paths(datadir, 'yelp')

subdir('domains')
subdir('dtd')
subdir('xslt')

install_data('org.gnome.yelp.gschema.xml',
             install_dir : join_paths(datadir, 'glib-2.0', 'schemas'))

# Validate GSchema
if glib_compile_schemas.found()
  test(
    'validate-gschema', glib_compile_schemas,
    args: [
      '--strict', '--dry-run', meson.current_build_dir()
    ],
  )
endif

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

install_subdir('icons',
               install_dir: datadir,
               exclude_files: [
                 'hicolor/scalable/apps/org.gnome.Yelp.Devel.svg',
               ])

metainfo_file = i18n.merge_file(
  input: 'org.gnome.Yelp.metainfo.xml.in',
  output: 'org.gnome.Yelp.metainfo.xml',
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
  po_dir: join_paths(meson.project_source_root(), 'po'),
  type: 'xml')

# Validate Metainfo
if appstreamcli.found()
  test(
    'validate-metainfo', appstreamcli,
    args: [
      'validate', '--no-net', '--explain', metainfo_file
    ],
    depends: metainfo_file,
  )
endif

gnome.post_install(glib_compile_schemas: true)
