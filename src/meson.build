subdir('platform')

srcInclude = include_directories('.')

subdir('admesh')
# subdir('avrdude')
subdir('boost/nowide')
subdir('clipper')
subdir('glu-libtess')
subdir('miniz')
subdir('poly2tri')
subdir('polypartition')
subdir('qhull')
subdir('semver')
subdir('Shiny')

subdir('libnest2d')

subdir('libslic3r')




if get_option('gui')
  subdir('imgui')

  if on_windows
    message('WXWIN environment set to: @0@'.format())
  endif
  if on_linux
    wxWidgets_USE_UNICODE = true
    wxWidgets_USE_STATIC = static_dependencies
  endif

  wx_version = (on_linux and get_option('wx_stable')) ? '>=3.0' : '>=3.1'

  wx_modules = [
    'base',
    'core',
    'adv',
    'html',
    'gl',
  ]

  if on_linux
    wx_modules += '--toolkit=gtk@0@'.format(get_option('wx_gtk_version'))
  endif

  wxwidgets = dependency('WxWidgets',
    modules: wx_modules,
    version: wx_version,
  )

  subdir('slic3r')
endif


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# Final executable

slic3r_target_name = 'slic3r-' + (get_option('gui') ? 'gui' : 'console')
slic3r_target_type = on_windows ? 'shared_library' : 'executable'

slic3r_platform_ldflags = [ ]
if on_appleos
  slic3r_platform_ldflags += [
    '-liconv -framework IOKit',
    '-framework CoreFoundation',
    '-lc++',
  ]
elif on_windows
  # Manifest is provided through slic3r.rc, don't generate your own.
  slic3r_platform_ldflags += [
    '/MANIFEST:NO',
  ]
else
  # Boost on Raspberry-Pi does not link to pthreads explicitely.
  ## FIXME What ?
  slic3r_platform_ldflags += [
    '-lstdc++',
    # 'Threads::Threads',
  ]
endif



slic3r = build_target(slic3r_target_name,
  'slic3r.cpp',
  target_type: slic3r_target_type,

  include_directories: [
    'platform',
    'libslic3r',
  ],

  link_with: [
    libslic3r,
  ],
  link_args: [
    slic3r_platform_ldflags,
  ],
  dependencies: [
    eigen,
    libnest2d,
    clipper,
    boost,
  ],
)
