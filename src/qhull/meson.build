# This file is written specifically to integrate qhull library with PrusaSlicer
# (see https://github.com/prusa3d/PrusaSlicer for more information)
#
# Only original libraries qhullstatic_r and qhullcpp are included.
# They are built as a single statically linked library.
#
# Created by modification of the original qhull CMakeLists.
# Lukas Matena (25.7.2018), lukasmatena@seznam.cz


# project(qhull)

# not used, See global.c, global_r.c, rbox.c, rbox_r.c
qhull_VERSION2= '2015.2 2016/01/18'
# Advance every release
qhull_VERSION = '7.2.0'


message('qhull Version: @0@ (static linking)'.format(qhull_VERSION))

libqhull_sources = [
  # reentrant qhull SOURCES:
  'src/libqhull_r/geom_r.c',
  'src/libqhull_r/geom2_r.c',
  'src/libqhull_r/global_r.c',
  'src/libqhull_r/io_r.c',
  'src/libqhull_r/libqhull_r.c',
  'src/libqhull_r/mem_r.c',
  'src/libqhull_r/merge_r.c',
  'src/libqhull_r/poly_r.c',
  'src/libqhull_r/poly2_r.c',
  'src/libqhull_r/qset_r.c',
  'src/libqhull_r/random_r.c',
  'src/libqhull_r/rboxlib_r.c',
  'src/libqhull_r/stat_r.c',
  'src/libqhull_r/user_r.c',
  'src/libqhull_r/usermem_r.c',

  # C++ interface to reentrant Qhull SOURCES:
  'src/libqhullcpp/Coordinates.cpp',
  'src/libqhullcpp/PointCoordinates.cpp',
  'src/libqhullcpp/Qhull.cpp',
  'src/libqhullcpp/QhullFacet.cpp',
  'src/libqhullcpp/QhullFacetList.cpp',
  'src/libqhullcpp/QhullFacetSet.cpp',
  'src/libqhullcpp/QhullHyperplane.cpp',
  'src/libqhullcpp/QhullPoint.cpp',
  'src/libqhullcpp/QhullPointSet.cpp',
  'src/libqhullcpp/QhullPoints.cpp',
  'src/libqhullcpp/QhullQh.cpp',
  'src/libqhullcpp/QhullRidge.cpp',
  'src/libqhullcpp/QhullSet.cpp',
  'src/libqhullcpp/QhullStat.cpp',
  'src/libqhullcpp/QhullVertex.cpp',
  'src/libqhullcpp/QhullVertexSet.cpp',
  'src/libqhullcpp/RboxPoints.cpp',
  'src/libqhullcpp/RoadError.cpp',
  'src/libqhullcpp/RoadLogEvent.cpp',
]

qhullInclude = include_directories('src')

qhull = static_library('qhull',
  libqhull_sources,

  include_directories: qhullInclude,
  dependencies: math,
)

# set_target_properties(${qhull_STATIC} PROPERTIES VERSION ${qhull_VERSION})
