webextension_sources = files(
  '../yelp-settings.c',
  '../yelp-uri-builder.c',
  '../yelp-uri.c',
  'yelp-web-extension.c',
)

webextension = shared_module('yelpwebextension',
                             sources: webextension_sources,
                             install: true,
                             link_args : ['-Wl,--no-undefined'],
                             dependencies: [webkitgtk_web_extension_dep],
                             include_directories: [configinc, libyelpinc],
                             c_args: ['-DHAVE_CONFIG_H'],
                             install_dir: join_paths(libdir,
                                                     'yelp',
                                                     'web-extensions'),
)
