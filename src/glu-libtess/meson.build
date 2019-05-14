glu_libtessInclude = include_directories('include')

glu_libtess = static_library('glu-libtess',
  'src/dict.c',
  'src/geom.c',
  'src/memalloc.c',
  'src/mesh.c',
  'src/normal.c',
  'src/priorityq.c',
  'src/render.c',
  'src/sweep.c',
  'src/tess.c',
  'src/tessmono.c',

  include_directories: glu_libtessInclude,

  dependencies: math,
)
