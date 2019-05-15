
project('libnest2d',
  'c',
)

nlopt = dependency('nlopt')

libnest2d = static_library('libnest2d',
)

dependency = declare_dependency(
  link_with: libnest2d,
  include_directories: 'include',
  compile_args: [
    '-DLIBNEST2D_BACKEND_CLIPPER',
    '-DUSE_TBB',
    '-DTBB_USE_CAPTURED_EXCEPTION=0',
    '-DLIBNEST2D_OPTIMIZER_NLOPT',
  ],
  dependencies: nlopt,
)
