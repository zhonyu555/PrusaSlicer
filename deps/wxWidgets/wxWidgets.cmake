set(_wx_git_tag v3.1.4-patched)

set(_wx_toolkit "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_gtk_ver 2)
    if (DEP_WX_GTK3)
        set(_gtk_ver 3)
    endif ()
    set(_wx_toolkit "-DwxBUILD_TOOLKIT=gtk${_gtk_ver}")
endif()

set(_wx_config_command "")
if (DEP_MSAN)
    set(_wx_cmake_args
            -DCMAKE_INSTALL_PREFIX:STRING=${DESTDIR}/usr/local
            -DCMAKE_MODULE_PATH:STRING=${PROJECT_SOURCE_DIR}/../cmake/modules
            -DCMAKE_PREFIX_PATH:STRING=${DESTDIR}/usr/local
            -DCMAKE_DEBUG_POSTFIX:STRING=d
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE}
            -DBUILD_SHARED_LIBS:BOOL=OFF
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            ${DEP_CMAKE_OPTS}
            -DwxBUILD_PRECOMP=ON
            ${_wx_toolkit}
            "-DCMAKE_DEBUG_POSTFIX:STRING="
            -DwxBUILD_DEBUG_LEVEL=0
            -DwxUSE_MEDIACTRL=OFF
            -DwxUSE_DETECT_SM=OFF
            -DwxUSE_UNICODE=ON
            -DwxUSE_OPENGL=ON
            -DwxUSE_LIBPNG=sys
            -DwxUSE_ZLIB=sys
            -DwxUSE_REGEX=builtin
            -DwxUSE_LIBXPM=builtin
            -DwxUSE_LIBJPEG=sys
            -DwxUSE_LIBTIFF=sys
            -DwxUSE_EXPAT=sys
            -DwxUSE_LIBSDL=OFF
            -DwxUSE_XTEST=OFF)
    set(_wx_config_command CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CFLAGS=${MSAN_CMAKE_C_FLAGS}" "CXXFLAGS=${MSAN_CMAKE_CXX_FLAGS}" "LDFLAGS=${MSAN_CMAKE_LD_FLAGS}" ${CMAKE_COMMAND}
            ${_wx_cmake_args}
            "${CMAKE_GENERATOR}"
            ${DEP_DOWNLOAD_DIR}/dep_wxWidgets-prefix/src/dep_wxWidgets)
endif ()

prusaslicer_add_cmake_project(wxWidgets
    # GIT_REPOSITORY "https://github.com/prusa3d/wxWidgets"
    # GIT_TAG tm_cross_compile #${_wx_git_tag}
    URL https://github.com/prusa3d/wxWidgets/archive/73f029adfcc82fb3aa4b01220a013f716e57d110.zip
    URL_HASH SHA256=c35fe0187db497b6a3f477e24ed5e307028657ff0c2554385810b6e7961ad2e4
    DEPENDS ${LIBCXX_PKG} ${FONTCONFIG_PKG} ${PNG_PKG} ${ZLIB_PKG} ${EXPAT_PKG} dep_TIFF dep_JPEG
    CMAKE_ARGS
        -DwxBUILD_PRECOMP=ON
        ${_wx_toolkit}
        "-DCMAKE_DEBUG_POSTFIX:STRING="
        -DwxBUILD_DEBUG_LEVEL=0
        -DwxUSE_MEDIACTRL=OFF
        -DwxUSE_DETECT_SM=OFF
        -DwxUSE_UNICODE=ON
        -DwxUSE_OPENGL=ON
        -DwxUSE_LIBPNG=sys
        -DwxUSE_ZLIB=sys
        -DwxUSE_REGEX=builtin
        -DwxUSE_LIBXPM=builtin
        -DwxUSE_LIBJPEG=sys
        -DwxUSE_LIBTIFF=sys
        -DwxUSE_EXPAT=sys
        -DwxUSE_LIBSDL=OFF
        -DwxUSE_XTEST=OFF
    ${_wx_config_command}
)

if (MSVC)
    add_debug_dep(dep_wxWidgets)
endif ()