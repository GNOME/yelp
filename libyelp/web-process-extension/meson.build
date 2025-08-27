webprocessextension_sources = files(
  '../yelp-settings.c',
  '../yelp-uri-builder.c',
  '../yelp-uri.c',
  'yelp-web-process-extension.c',
)

webprocessextension = library('yelpwebprocessextension',
                             sources: webprocessextension_sources,
                             install: true,
                             link_args : ['-Wl,--no-undefined'],
                             dependencies: [libadwaita_dep, webkitgtk_web_process_extension_dep],
                             include_directories: [configinc, libyelpinc],
                             c_args: ['-DHAVE_CONFIG_H'],
                             install_dir: yelp_web_process_extensions_dir,
)
