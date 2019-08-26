yelp_sources = files(
  'yelp-application.c',
  'yelp-window.c',
  'yelp.c',
)

yelp = executable('yelp',
                  sources: yelp_sources,
                  c_args: [
                    '-DHAVE_CONFIG_H',
                    '-DG_LOG_DOMAIN="Yelp"',
                  ],
                  dependencies: [
                    libyelp_dep,
                  ],
                  install: true,
                  include_directories: [configinc],
)

meson.add_install_script('link-gnome-help.sh')
