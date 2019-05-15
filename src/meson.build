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

slicer_target_name = 'slicer-' + (get_option('gui') ? 'gui' : 'console')
slicer_target_type = on_windows ? 'shared_library' : 'executable'

slicer_platform_ldflags = [ ]
if on_appleos
  slicer_platform_ldflags += [
    '-liconv -framework IOKit',
    '-framework CoreFoundation',
    '-lc++',
  ]
elif on_windows
  # Manifest is provided through slicer.rc, don't generate your own.
  slicer_platform_ldflags += [
    '/MANIFEST:NO',
  ]
else
  # Boost on Raspberry-Pi does not link to pthreads explicitely.
  ## FIXME What ?
  slicer_platform_ldflags += [
    '-lstdc++',
    # 'Threads::Threads',
  ]
endif



slicer = build_target(slicer_target_name,
  'slic3r.cpp',
  target_type: slic3r_target_type,

  include_directories: [
    'platform',
    'libslic3r',
  ],

  link_with: [
    libslicer,
    libslicer_gui,
  ],
  link_args: [
    slicer_platform_ldflags,
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
# version of the PrusaSlicer application.
# Also the shim may load the Mesa software OpenGL renderer if the default
# renderer does not support OpenGL 2.0 and higher.
if on_windows
  slicer_app_gui = executable('slicer',
    'slic3r_app_msvc.cpp',
    slicer_rc,

    cpp_args: [
      '-DSLIC3R_WRAPPER_NOCONSOLE',
    ],
    depends: slicer,
  )
  slicer_app_console = executable('slicer-console',
    'slic3r_app_msvc.cpp',
    slicer_rc,

    cpp_args: [
      '-DSLIC3R_WRAPPER_CONSOLE',
    ],
    depends: slicer,
  )
endif
