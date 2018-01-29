libyelp_deps = [
  gio_dep,
  gio_unix_dep,
  gtk_dep,
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
  'yelp-search-entry.h',
  'yelp-settings.h',
  'yelp-simple-document.h',
  'yelp-sqlite-storage.h',
  'yelp-storage.h',
  'yelp-transform.h',
  'yelp-uri-builder.h',
  'yelp-uri.h',
  'yelp-view.h',
)
install_headers(libyelp_headers, subdir: 'libyelp')

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
  'yelp-search-entry.c',
  'yelp-settings.c',
  'yelp-simple-document.c',
  'yelp-sqlite-storage.c',
  'yelp-storage.c',
  'yelp-transform.c',
  'yelp-uri-builder.c',
  'yelp-uri.c',
  'yelp-view.c',
)

if enable_lzma
  libyelp_sources += files('yelp-lzma-decompressor.c')
  libyelp_deps += lzma_dep
endif

if enable_bzip2
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
                              install_dir: join_paths(includedir, 'libyelp'),
                              sources: libyelp_headers,
)

libyelp = library('yelp',
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
                                   webkitgtk_dep,
                                 ],
)

subdir('web-extension')
