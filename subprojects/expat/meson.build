
project('expat',
  'c',
  version: '2.2.0',
)

libexpat = static_library('expat',
  'xmlparse.c',
  'xmlrole.c',
  'xmltok.c',
),

dependency = declare_dependency(
  link_with: libexpat,
  include_directories: include_directories('.')
)
