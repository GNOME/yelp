project('yelp', 'c',
        version : '3.30.0',
        meson_version : '>= 0.49.0',
        default_options : [
          'warning_level=1',
          'c_std=gnu89'
        ]
)

prefix = get_option('prefix')
localedir = get_option('localedir')
datadir = get_option('datadir')
libdir = get_option('libdir')
includedir = get_option('includedir')
buildtype = get_option('buildtype')

yelp_conf = configuration_data()

if buildtype == 'debug' or buildtype == 'debugoptimized'
  yelp_conf.set('YELP_DEBUG', true)
endif
yelp_conf.set_quoted('PACKAGE', 'yelp')
yelp_conf.set_quoted('GETTEXT_PACKAGE', 'yelp')
yelp_conf.set_quoted('VERSION', meson.project_version())
yelp_conf.set_quoted('LOCALEDIR', join_paths(prefix, localedir))
yelp_conf.set_quoted('DATADIR', join_paths(prefix, datadir))
# TODO: unquoted PACKAGE_VERSION for version.xml?

gio_dep_req = '>= 2.38.0'
gtk_dep_req = '>= 3.13.3'
libxml_dep_req = '>= 2.6.5'
libxslt_dep_req = '>= 1.1.4'
libexslt_dep_req = '>= 0.8.1'
webkitgtk_dep_req = '>= 2.19.2'
lzma_dep_req = '>= 4.9'
bzip2_dep_req = '>= 1.0'
yelp_xsl_dep_req = '>= 3.27.1'

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required : false)
gio_dep = dependency('gio-2.0', version: gio_dep_req)
gio_unix_dep = dependency('gio-unix-2.0')
gtk_dep = dependency('gtk+-3.0', version: gtk_dep_req)
libxml_dep = dependency('libxml-2.0', version: libxml_dep_req)
libxslt_dep = dependency('libxslt', version: libxslt_dep_req)
libexslt_dep = dependency('libexslt', version: libexslt_dep_req)
sqlite_dep = dependency('sqlite3')
webkitgtk_dep = dependency('webkit2gtk-4.0', version: webkitgtk_dep_req)
yelp_xsl_dep = dependency('yelp-xsl', version: yelp_xsl_dep_req)
webkitgtk_web_extension_dep = dependency('webkit2gtk-web-extension-4.0',
                                         version: webkitgtk_dep_req)

lzma_dep = dependency('liblzma',
                      version: lzma_dep_req,
                      required: get_option('lzma'))
if lzma_dep.found()
  yelp_conf.set('ENABLE_LZMA', true)
endif

if get_option('bzip2').enabled()
  # Do not fail the configuration step even if we explicitly enabled
  # bzip2. If there is no pkg-config file, we will check for an
  # existence of the library and that will be a point of failure when
  # library is really absent on the system.
  bzip2_dep = dependency('bzip2',
                         version: bzip2_dep_req,
                         required: false)
else
  bzip2_dep = dependency('bzip2',
                         version: bzip2_dep_req,
                         required: get_option('bzip2'))
endif
if not bzip2_dep.found()
  bzip2_dep = cc.find_library('bz2',
                              required: get_option('bzip2'))
endif
if bzip2_dep.found()
  yelp_conf.set('ENABLE_BZ2', true)
endif

itstool = find_program('itstool')
r = run_command(itstool, '--version')
if r.returncode() != 0
  error('failed to run itstool --version')
endif
parts = r.stdout().strip().split()
if parts.length() != 2
  error('unexpected output from itstool --version')
endif
if parts[1].version_compare('<1.2.0')
  error('itstool >= 1.2.0 is required to build the translation XML files')
endif

xsl_xsltdir = yelp_xsl_dep.get_pkgconfig_variable('xsltdir')
xsl_icondir = yelp_xsl_dep.get_pkgconfig_variable('icondir')
xsl_jsdir = yelp_xsl_dep.get_pkgconfig_variable('jsdir')
xsl_db2xhtml = yelp_xsl_dep.get_pkgconfig_variable('db2xhtml')
xsl_mal2xhtml = yelp_xsl_dep.get_pkgconfig_variable('mal2xhtml')
xsl_gettext = yelp_xsl_dep.get_pkgconfig_variable('gettext')

xsl_color = '@0@/common/color.xsl'.format(xsl_xsltdir)
xsl_icons = '@0@/common/icons.xsl'.format(xsl_xsltdir)
xsl_html = '@0@/common/html.xsl'.format(xsl_xsltdir)

yelp_xsl_conf = configuration_data()

yelp_xsl_conf.set('XSL_DB2XHTML', xsl_db2xhtml)
yelp_xsl_conf.set('XSL_COLOR', xsl_color)
yelp_xsl_conf.set('XSL_ICONS', xsl_icons)
yelp_xsl_conf.set('XSL_HTML', xsl_html)
yelp_xsl_conf.set('XSL_GETTEXT', xsl_gettext)
yelp_xsl_conf.set('XSL_MAL2XHTML', xsl_mal2xhtml)
yelp_xsl_conf.set('XSL_JSDIR', xsl_jsdir)
yelp_xsl_conf.set('DATADIR', join_paths(prefix, datadir))

yelp_conf.set_quoted('YELP_ICON_PATH', xsl_icondir)
yelp_conf.set_quoted('YELP_WEB_EXTENSIONS_DIR',
                     join_paths(prefix, libdir, 'yelp', 'web-extensions'))

configure_file(output: 'config.h',
               configuration: yelp_conf)

configinc = include_directories('.')
libyelpinc = include_directories('libyelp')

gnome = import('gnome')
i18n = import('i18n')

i18n.merge_file('desktop',
                input: 'yelp.desktop.in',
                output: 'yelp.desktop',
                install: true,
                install_dir: join_paths(datadir, 'applications'),
                po_dir: join_paths(meson.source_root(), 'po'),
                type: 'desktop')

subdir('libyelp')
subdir('src')
subdir('data')
subdir('po')
if get_option('gtk_doc')
  subdir('docs/libyelp')
endif
