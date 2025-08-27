pkg = import('pkgconfig')

libyelp_deps = [
  m_dep,
  gio_dep,
  gio_unix_dep,
  gtk_dep,
  libadwaita_dep,
  libxml_dep,
  libxslt_dep,
  libexslt_dep,
  sqlite_dep,
  webkitgtk_dep,
  yelp_xsl_dep,
]

libyelp_headers = files(
  'yelp-bookmarks.h',
  'yelp-docbook-document.h',
  'yelp-document.h',
  'yelp-help-list.h',
  'yelp-info-document.h',
  'yelp-mallard-document.h',
  'yelp-man-document.h',
  'yelp-man-search.h',
  'yelp-settings.h',
  'yelp-simple-document.h',
  'yelp-sqlite-storage.h',
  'yelp-storage.h',
  'yelp-transform.h',
  'yelp-uri-builder.h',
  'yelp-uri.h',
  'yelp-view.h',
)

headers_path = 'libyelp-@0@'.format(lib_version)
install_headers(libyelp_headers, subdir: headers_path)

libyelp_sources = files(
  'yelp-bookmarks.c',
  'yelp-debug.c',
  'yelp-docbook-document.c',
  'yelp-document.c',
  'yelp-error.c',
  'yelp-help-list.c',
  'yelp-info-document.c',
  'yelp-info-parser.c',
  'yelp-magic-decompressor.c',
  'yelp-mallard-document.c',
  'yelp-man-document.c',
  'yelp-man-parser.c',
  'yelp-man-search.c',
  'yelp-settings.c',
  'yelp-simple-document.c',
  'yelp-sqlite-storage.c',
  'yelp-storage.c',
  'yelp-transform.c',
  'yelp-uri-builder.c',
  'yelp-uri.c',
  'yelp-view.c',
)

if lzma_dep.found()
  libyelp_sources += files('yelp-lzma-decompressor.c')
  libyelp_deps += lzma_dep
endif

if bzip2_dep.found()
  libyelp_sources += files('yelp-bz2-decompressor.c')
  libyelp_deps += bzip2_dep
endif

libyelp_marshallers = gnome.genmarshal('yelp-marshal',
                                       sources: 'yelp-marshal.list',
                                       prefix: 'yelp_marshal',
                                       internal: true,
)

libyelp_enums = gnome.mkenums('yelp-types',
                              c_template: 'yelp-types.c.template',
                              h_template: 'yelp-types.h.template',
                              install_header: true,
                              install_dir: join_paths(includedir, headers_path),
                              sources: libyelp_headers,
)

libyelp = library('yelp-@0@'.format(lib_version),
                  sources: libyelp_sources + libyelp_marshallers + libyelp_enums,
                  install: true,
                  soversion: '0',
                  version: '0.0.0',
                  dependencies: libyelp_deps,
                  include_directories: configinc,
                  c_args: [
                    '-DHAVE_CONFIG_H',
                  ],
)

libyelp_dep = declare_dependency(link_with: libyelp,
                                 include_directories: libyelpinc,
                                 dependencies: [
                                   gio_dep,
                                   gtk_dep,
                                   libadwaita_dep,
                                   webkitgtk_dep,
                                 ],
)

pkg.generate(libyelp, name: headers_path, description: 'Yelp library',
    subdirs: headers_path)

subdir('web-process-extension')
