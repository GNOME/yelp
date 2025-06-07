gen_yelp_pot = find_program('gen_yelp_pot.sh')
gen_yelp_xml = find_program('gen_yelp_xml.sh')
msgfmt = find_program('msgfmt')

run_target('yelp.pot',
           command: [
             gen_yelp_pot,
             '--itstool', itstool.full_path(),
           ])

custom_target('yelp-xml-domain',
              input: 'yelp.xml.in',
              output: 'yelp.xml',
              command: [
                gen_yelp_xml,
                '--po-dir', join_paths(meson.project_source_root(), 'po'),
                '--itstool', itstool,
                '--msgfmt', msgfmt,
                '--input', '@INPUT@',
                '--output', '@OUTPUT@',
                '--target', 'yelp-xml-domain',
              ],
              install: true,
              install_dir: join_paths(datadir,
                                      'yelp-xsl',
                                      'xslt',
                                      'common',
                                      'domains'))
