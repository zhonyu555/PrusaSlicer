if(BUILD_SHARED_LIBS)
    set(_build_shared ON)
    set(_build_static OFF)
else()
    set(_build_shared OFF)
    set(_build_static ON)
endif()

set (_openvdb_vdbprint ON)
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
    # Build fails on raspberry pi due to missing link directive to latomic
    # Let's hope it will be fixed soon.
    set (_openvdb_vdbprint OFF)
endif ()

prusaslicer_add_cmake_project(OpenVDB
    # 8.2 patched
    URL https://github.com/tamasmeszaros/openvdb/archive/a68fd58d0e2b85f01adeb8b13d7555183ab10aa5.zip
    URL_HASH SHA256=f353e7b99bd0cbfc27ac9082de51acf32a8bc0b3e21ff9661ecca6f205ec1d81
    DEPENDS dep_TBB dep_Blosc dep_OpenEXR dep_Boost
    CMAKE_ARGS
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON 
        -DOPENVDB_BUILD_PYTHON_MODULE=OFF
        -DUSE_BLOSC=ON
        -DOPENVDB_CORE_SHARED=${_build_shared} 
        -DOPENVDB_CORE_STATIC=${_build_static}
        -DOPENVDB_ENABLE_RPATH:BOOL=OFF
        -DTBB_STATIC=${_build_static}
        -DOPENVDB_BUILD_VDB_PRINT=${_openvdb_vdbprint}
        -DDISABLE_DEPENDENCY_VERSION_CHECKS=ON # Centos6 has old zlib
)

ExternalProject_Get_Property(dep_OpenVDB BINARY_DIR)
if (MSVC)
    if (${DEP_DEBUG})
        ExternalProject_Add_Step(dep_OpenVDB build_debug
            DEPENDEES build
            DEPENDERS install
            COMMAND ${CMAKE_COMMAND} ../dep_OpenVDB -DOPENVDB_BUILD_VDB_PRINT=OFF
            COMMAND msbuild /m /P:Configuration=Debug INSTALL.vcxproj
            WORKING_DIRECTORY "${BINARY_DIR}"
        )
    endif ()
elseif (APPLE)
    # Fix zstd not being found during linking
    find_program(HOMEBREW_EXECUTABLE brew NO_CACHE)
    if(HOMEBREW_EXECUTABLE STREQUAL "HOMEBREW_EXECUTABLE-NOTFOUND")
        message(FATAL_ERROR "Homebrew must be installed in order to install zstd.")
    endif ()

    execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix zstd
                    RESULT_VARIABLE HOMEBREW_ZSTD_PREFIX_RESULT
                    OUTPUT_VARIABLE HOMEBREW_ZSTD_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT HOMEBREW_ZSTD_PREFIX_RESULT EQUAL "0")
        message(FATAL_ERROR "Could not find the zstd Homebrew keg. Ensure that zstd is installed via homebrew.")
    endif ()

    message(STATUS "Found zstd Homebrew prefix: ${HOMEBREW_ZSTD_PREFIX}")

    ExternalProject_Add_Step(dep_OpenVDB fix_zstd_linking
        DEPENDEES configure
        DEPENDERS build
        COMMAND ${CMAKE_COMMAND} ../dep_OpenVDB -DCMAKE_EXE_LINKER_FLAGS:STRING=-L${HOMEBREW_ZSTD_PREFIX}/lib
        WORKING_DIRECTORY "${BINARY_DIR}"
    )
endif ()
