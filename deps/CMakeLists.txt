#
# This CMake project downloads, configures and builds PrusaSlicer dependencies on Unix and Windows.
#
# When using this script, it's recommended to perform an out-of-source build using CMake.
#
# All the dependencies are installed in a `destdir` directory in the root of the build directory,
# in a traditional Unix-style prefix structure. The destdir can be used directly by CMake
# when building PrusaSlicer - to do this, set the CMAKE_PREFIX_PATH to ${destdir}/usr/local.
# Warning: On UNIX/Linux, you also need to set -DSLIC3R_STATIC=1 when building PrusaSlicer.
#
# For better clarity of console output, it's recommended to _not_ use a parallelized build
# for the top-level command, ie. use `make -j 1` or `ninja -j 1` to force single-threaded top-level
# build. This doesn't degrade performance as individual dependencies are built in parallel fashion
# if supported by the dependency.
#
# On Windows, architecture (64 vs 32 bits) is judged based on the compiler variant.
# To build dependencies for either 64 or 32 bit OS, use the respective compiler command line.
#
# WARNING: On UNIX platforms wxWidgets hardcode the destdir path into its `wx-conffig` utility,
# therefore, unfortunatelly, the installation cannot be copied/moved elsewhere without re-installing wxWidgets.
#

project(PrusaSlicer-deps)
cmake_minimum_required(VERSION 3.2)

include(ExternalProject)
include(ProcessorCount)

ProcessorCount(NPROC)
if (NPROC EQUAL 0)
    set(NPROC 1)
endif ()

set(DESTDIR "${CMAKE_CURRENT_BINARY_DIR}/destdir" CACHE PATH "Destination directory")
set(DEP_DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR} CACHE PATH "Path for downloaded source packages.")

option(DEP_DEBUG "Build debug variants (only applicable on Windows)" ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(DEP_WX_GTK3 "Build wxWidgets against GTK3" OFF)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(DEP_MSAN                 "Enable MemorySanitizer on Clang for Linux" OFF)
    option(DEP_MSAN_RECOVERY        "Enable recovery from an error after an initialized value is found" ON) # It is enabled by default until all reports are investigated.
    option(DEP_MSAN_ORIGIN_TRACKING "Enable tracking origins of uninitialized values" OFF) # It causes another slows down of program execution by a factor of 1.5x-2x.
endif()

# On developer machines, it can be enabled to speed up compilation and suppress warnings coming from IGL. 
# FIXME:
# Enabling this option is not safe. IGL will compile itself with its own version of Eigen while
# Slic3r compiles with a different version which will cause runtime errors.
# option(DEP_BUILD_IGL_STATIC "Build IGL as a static library. Might cause link errors and increase binary size." OFF)

message(STATUS "PrusaSlicer deps DESTDIR: ${DESTDIR}")
message(STATUS "PrusaSlicer dowload dir for source packages: ${DEP_DOWNLOAD_DIR}")
message(STATUS "PrusaSlicer deps debug build: ${DEP_DEBUG}")

find_package(Git REQUIRED)

# The default command line for patching. Only works for newer 
set(PATCH_CMD ${GIT_EXECUTABLE} apply --verbose --ignore-space-change --whitespace=fix)

get_property(_is_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if (NOT _is_multi AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Forcing CMAKE_BUILD_TYPE to Release as it was not specified.")
endif ()

function(prusaslicer_add_cmake_project projectname)
    cmake_parse_arguments(P_ARGS "" "INSTALL_DIR;BUILD_COMMAND;INSTALL_COMMAND" "CMAKE_ARGS" ${ARGN})

    set(_configs_line -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE})
    if (_is_multi OR MSVC)
        set(_configs_line "")
    endif ()

    set(_gen "")
    set(_build_j "-j${NPROC}")
    if (MSVC)
        set(_gen CMAKE_GENERATOR "${DEP_MSVC_GEN}" CMAKE_GENERATOR_PLATFORM "${DEP_PLATFORM}")
        set(_build_j "/m")
    endif ()

    ExternalProject_Add(
        dep_${projectname}
        EXCLUDE_FROM_ALL    ON
        INSTALL_DIR         ${DESTDIR}/usr/local
        DOWNLOAD_DIR        ${DEP_DOWNLOAD_DIR}/${projectname}
        DEPENDS             ${RUNTIME_LIBS}
        ${_gen}
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:STRING=${DESTDIR}/usr/local
            -DCMAKE_MODULE_PATH:STRING=${PROJECT_SOURCE_DIR}/../cmake/modules
            -DCMAKE_PREFIX_PATH:STRING=${DESTDIR}/usr/local
            -DCMAKE_DEBUG_POSTFIX:STRING=d
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE}
            -DBUILD_SHARED_LIBS:BOOL=OFF
            "${_configs_line}"
            ${DEP_CMAKE_OPTS}
            ${P_ARGS_CMAKE_ARGS}
       ${P_ARGS_UNPARSED_ARGUMENTS}
       BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release -- ${_build_j}
       INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config Release
    )

endfunction(prusaslicer_add_cmake_project)


if (MSVC)
    if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        message(STATUS "\nDetected 64-bit compiler => building 64-bit deps bundle\n")
        set(DEPS_BITS 64)
        include("deps-windows.cmake")
    elseif ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        message(STATUS "\nDetected 32-bit compiler => building 32-bit deps bundle\n")
        set(DEPS_BITS 32)
        include("deps-windows.cmake")
    else ()
        message(FATAL_ERROR "Unable to detect architecture")
    endif ()
elseif (APPLE)
    message("OS X SDK Path: ${CMAKE_OSX_SYSROOT}")
    if (CMAKE_OSX_DEPLOYMENT_TARGET)
        set(DEP_OSX_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        message("OS X Deployment Target: ${DEP_OSX_TARGET}")
    else ()
        # Attempt to infer the SDK version from the CMAKE_OSX_SYSROOT,
        # this is done because wxWidgets need the min version explicitly set
        string(REGEX MATCH "[0-9]+[.][0-9]+[.]sdk$" DEP_OSX_TARGET "${CMAKE_OSX_SYSROOT}")
        string(REGEX MATCH "^[0-9]+[.][0-9]+" DEP_OSX_TARGET "${DEP_OSX_TARGET}")

        if (NOT DEP_OSX_TARGET)
            message(FATAL_ERROR "Could not determine OS X SDK version. Please use -DCMAKE_OSX_DEPLOYMENT_TARGET=<version>")
        endif ()

        message("OS X Deployment Target (inferred from SDK): ${DEP_OSX_TARGET}")
    endif ()

    include("deps-macos.cmake")
elseif (MINGW)
    message(STATUS "Building for MinGW...")
    include("deps-mingw.cmake")
else()
    include("deps-linux.cmake")
endif()

if (DEP_MSAN)
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        message(FATAL_ERROR "MemorySanitizer is available only in Clang.")
    endif ()

    if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 12)
        message(WARNING "MemorySanitizer builds are tested only with CLANG 12, other versions are untested but could be working.")
    endif()

    file(MAKE_DIRECTORY ${DESTDIR}/usr/lib)

    include(MSan_Deps/Libcxx/Libcxx.cmake)
    include(MSan_Deps/Chromium_prebuilt_libraries/Chromium.cmake)
    include(MSan_Deps/LLVM/LLVM.cmake)
    include(MSan_Deps/Mesa3D/Mesa3D.cmake)
    include(MSan_Deps/fontconfig/fontconfig.cmake)

    set(_msan_linker_flags "-fsanitize=memory")
    if (DEP_MSAN_RECOVERY)
        set(_msan_linker_flags "${_msan_linker_flags} -fsanitize-recover=memory")
    endif ()
    if (DEP_MSAN_ORIGIN_TRACKING)
        set(_msan_linker_flags "${_msan_linker_flags} -fsanitize-memory-track-origins=2")
    endif ()
    set(_msan_common_flags   "${_msan_linker_flags} -fno-omit-frame-pointer")
    set(_msan_only_cxx_flags "-nostdinc++ -isystem ${DESTDIR}/usr/local/include -isystem ${DESTDIR}/usr/local/include/c++/v1")
    set(_msan_only_ld_flags  "-Wl,-L${DESTDIR}/usr/local/lib,-lc++")

    # For dependencies that are configure using CFLAGS, CXXFLAGS and LDFLAGS instead of CMAKE_*_FLAGS.
    set(MSAN_CMAKE_C_FLAGS   "${_msan_common_flags}")
    set(MSAN_CMAKE_CXX_FLAGS "${_msan_common_flags} ${_msan_only_cxx_flags}")
    set(MSAN_CMAKE_LD_FLAGS  "${_msan_common_flags} ${_msan_only_ld_flags}")

    set(_msan_all_flags  "${_msan_common_flags} ${_msan_only_cxx_flags} ${_msan_only_ld_flags}")
    
    set(MSAN_CMAKE_ARGS ${_MSAN_CMAKE_ARGS} -DCMAKE_C_FLAGS:STRING=${MSAN_CMAKE_C_FLAGS} -DCMAKE_CXX_FLAGS:STRING=${MSAN_CMAKE_CXX_FLAGS} -DCMAKE_EXE_LINKER_FLAGS=${_msan_only_ld_flags} -DCMAKE_SHARED_LINKER_FLAGS=${_msan_only_ld_flags} -DCMAKE_BUILD_RPATH:STRING=${DESTDIR}/usr/local/lib)

    set(DEP_CMAKE_OPTS ${DEP_CMAKE_OPTS} ${MSAN_CMAKE_ARGS})

    set(LIBCXX_PKG dep_Libcxx)
    set(FONTCONFIG_PKG dep_fontconfig)

    set(RUNTIME_LIBS ${LIBCXX_PKG})
else ()
    set(MSAN_CMAKE_ARGS "")
    set(LIBCXX_PKG "")
    set(FONTCONFIG_PKG "")
endif ()

set(ZLIB_PKG "")
if (NOT ZLIB_FOUND AND NOT DEP_MSAN)
    include(ZLIB/ZLIB.cmake)
    set(ZLIB_PKG dep_ZLIB)
endif ()
set(PNG_PKG "")
if (NOT PNG_FOUND AND NOT DEP_MSAN)
    include(PNG/PNG.cmake)
    set(PNG_PKG dep_PNG)
endif ()
set(EXPAT_PKG "")
if (NOT EXPAT_FOUND AND NOT DEP_MSAN)
    include(EXPAT/EXPAT.cmake)
    set(EXPAT_PKG dep_EXPAT)
endif ()

set(DEP_Boost_COMPONENTS system iostreams filesystem thread log locale regex date_time)
include(Boost/Boost.cmake)

# The order of includes respects the dependencies between libraries
include(Cereal/Cereal.cmake)
include(Qhull/Qhull.cmake)
include(GLEW/GLEW.cmake)
include(OpenCSG/OpenCSG.cmake)

include(TBB/TBB.cmake)

include(Blosc/Blosc.cmake)
include(OpenEXR/OpenEXR.cmake)
include(OpenVDB/OpenVDB.cmake)

include(GMP/GMP.cmake)
include(MPFR/MPFR.cmake)
include(CGAL/CGAL.cmake)

include(NLopt/NLopt.cmake)

include(OpenSSL/OpenSSL.cmake)
include(CURL/CURL.cmake)

if(NOT JPEG_FOUND AND NOT DEP_MSAN)
   include(JPEG/JPEG.cmake)
   set(JPEG_PKG dep_JPEG)
endif ()
include(TIFF/TIFF.cmake)
include(wxWidgets/wxWidgets.cmake)

set(_dep_list
    dep_Boost
    dep_TBB
    dep_CURL
    dep_wxWidgets
    dep_Cereal
    dep_NLopt
    dep_OpenVDB
    dep_OpenCSG
    dep_CGAL
    dep_Qhull
    ${PNG_PKG}
    ${ZLIB_PKG}
    ${EXPAT_PKG}
    )

# if (NOT MSVC)
    # Not working, static build has different Eigen
    #list(APPEND _dep_list "dep_libigl")
# endif()

if (DEP_MSAN)
    list(APPEND _dep_list "dep_LLVM;dep_Mesa3D;dep_fontconfig;dep_Chromium_Libs")
endif ()

add_custom_target(deps ALL DEPENDS ${_dep_list})

# Note: I'm not using any of the LOG_xxx options in ExternalProject_Add() commands
# because they seem to generate bogus build files (possibly a bug in ExternalProject).
