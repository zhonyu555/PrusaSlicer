
nlopt = dependency('nlopt')

libnest2d = declare_dependency(
  link_with: [],
  include_directories: 'include',
  compile_args: [
    '-DLIBNEST2D_BACKEND_CLIPPER',
    '-DUSE_TBB',
    '-DTBB_USE_CAPTURED_EXCEPTION=0',
    '-DLIBNEST2D_OPTIMIZER_NLOPT',
  ],
  dependencies: nlopt,
)
