################################################################################
# General Informations
################################################################################

cmake_minimum_required(VERSION 3.0)
project(MeshFix)

################################################################################

if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "No build type selected, default to Release")
	set(CMAKE_BUILD_TYPE "Release")
endif()

################################################################################

set(SOURCES
		src/TMesh/edge.cpp
		src/TMesh/vertex.cpp
		src/TMesh/io.cpp
		src/TMesh/triangle.cpp
		src/TMesh/tin.cpp
		src/Algorithms/detectIntersections.cpp
		src/Algorithms/holeFilling.cpp
		src/Algorithms/marchIntersections.cpp
		src/Algorithms/checkAndRepair.cpp
		src/Kernel/heap.cpp
		src/Kernel/matrix.cpp
		src/Kernel/orientation.c
		src/Kernel/list.cpp
		src/Kernel/coordinates.cpp
		src/Kernel/tmesh.cpp
		src/Kernel/graph.cpp
		src/Kernel/point.cpp
		src/Kernel/jqsort.cpp
		src/MeshFix/meshfix.cpp
)

################################################################################

add_library(meshfix ${SOURCES})

target_include_directories(meshfix PUBLIC
		include/TMesh
		include/Kernel
)

target_compile_options(meshfix PRIVATE -fpermissive)

################################################################################
