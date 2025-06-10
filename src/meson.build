conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

config_h_in = configure_file(
  output: 'yelp-config.h.in',
  configuration: conf,
)
config_h = vcs_tag(
  input: config_h_in,
  output: 'yelp-config.h',
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'Devel'
)

yelp_sources = files(
  'yelp-application.c',
  'yelp-search-result.c',
  'yelp-search-entry.c',
  'yelp-window.c',
  'yelp.c',
)
yelp_sources += config_h

yelp_sources += gnome.compile_resources('yelp-resources',
  'yelp.gresource.xml',
  c_name: 'yelp'
)

yelp = executable('yelp',
                  sources: yelp_sources,
                  c_args: [
                    '-DHAVE_CONFIG_H',
                    '-DG_LOG_DOMAIN="Yelp"',
                  ],
                  dependencies: [
                    m_dep,
                    libyelp_dep,
                    libadwaita_dep,
                  ],
                  install: true,
                  include_directories: [configinc],
)

meson.add_install_script('link-gnome-help.sh')
