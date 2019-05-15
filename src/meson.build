subdir('platform')

srcInclude = include_directories('.')

subdir('admesh')
subdir('boost/nowide')
subdir('clipper')
subdir('glu-libtess')
subdir('miniz')
subdir('poly2tri')
subdir('polypartition')
subdir('qhull')
subdir('semver')
subdir('Shiny')

subdir('libslic3r')

if get_option('gui')
  subdir('imgui')
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
    libslic3r_gui,
  ],
  link_args: [
    slic3r_platform_ldflags,
  ],
  dependencies: [
    eigen,
    libnest2d,
    clipper,
    boost,
    curl,
    zlib,
    gl,
    get_option('gui') ? wxwidgets : [],
  ],
)


# On Windows, a shim application is required to produce a console / non console
# version of the Slic3r application.
# Also the shim may load the Mesa software OpenGL renderer if the default
# renderer does not support OpenGL 2.0 and higher.
if on_windows
  slic3r_app_gui = executable('slic3r',
    'slic3r_app_msvc.cpp',
    slic3r_rc,

    cpp_args: [
      '-DSLIC3R_WRAPPER_NOCONSOLE',
    ],
    depends: slic3r,
  )
  slic3r_app_console = executable('slic3r-console',
    'slic3r_app_msvc.cpp',
    slic3r_rc,

    cpp_args: [
      '-DSLIC3R_WRAPPER_CONSOLE',
    ],
    depends: slic3r,
  )
endif
