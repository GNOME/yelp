private_headers = [
  'yelp-debug.h',
  'yelp-types.h',
  'yelp-common-types.h',
  'yelp-bz2-decompressor.h',
  'yelp-lzma-decompressor.h',
  'yelp-magic-decompressor.h',
]

version_conf = configuration_data()
version_conf.set('PACKAGE_VERSION', meson.project_version())

configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)
docpath = join_paths(prefix, datadir, 'gtk-doc', 'html')

gnome.gtkdoc('libyelp',
             mode: 'none',
             main_xml: 'libyelp-docs.xml',
             src_dir: [
               join_paths(meson.project_source_root(), 'libyelp'),
             ],
             dependencies: libyelp_dep,
             gobject_typesfile: join_paths(meson.project_source_root(), 'docs', 'libyelp', 'libyelp.types'),
             scan_args: [
               #'--ignore-decorators=_GDK_EXTERN|G_GNUC_WARN_UNUSED_RESULT',
               '--ignore-headers=' + ' '.join(private_headers),
             ],
             fixxref_args: [
               '--html-dir=@0@'.format(docpath),
             #  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
             #  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
             #  '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
             #  '--extra-dir=@0@'.format(cairo_docpath),
             ],
             #html_assets: images,
             install: true)
