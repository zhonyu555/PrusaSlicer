set(_wx_toolkit "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_gtk_ver 2)
    if (DEP_WX_GTK3)
        set(_gtk_ver 3)
    endif ()
    set(_wx_toolkit "-DwxBUILD_TOOLKIT=gtk${_gtk_ver}")
endif()

# This is a needed submodule that is not fetched in the wxWidgets source archive 
# (because it's not an official release archive)
ExternalProject_Add(dep_NanoSVG
    EXCLUDE_FROM_ALL 1
    URL "https://github.com/wxWidgets/nanosvg/archive/refs/heads/wx.zip"
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/NanoSVG-src
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

set(_unicode_utf8 OFF)
if (UNIX) # wxWidgets will not use char as the underlying type for wxString unless its forced to.
    set (_unicode_utf8 ON)
endif()

prusaslicer_add_cmake_project(wxWidgets
    URL https://github.com/prusa3d/wxWidgets/archive/693b9dc2e1a73054cfff5fa40f08c26504ef3e32.zip
    URL_HASH SHA256=68d94c99729799a549f074f763507dd91b5e632d110f7bcbbbadd95dbdf1a64c
    DEPENDS ${PNG_PKG} ${ZLIB_PKG} ${EXPAT_PKG} dep_TIFF dep_JPEG dep_NanoSVG
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/NanoSVG-src 3rdparty/nanosvg 
    CMAKE_ARGS
        -DwxBUILD_PRECOMP=ON
        ${_wx_toolkit}
        "-DCMAKE_DEBUG_POSTFIX:STRING="
        -DwxBUILD_DEBUG_LEVEL=0
        -DwxUSE_MEDIACTRL=OFF
        -DwxUSE_DETECT_SM=OFF
        -DwxUSE_UNICODE=ON
        -DwxUSE_UNICODE_UTF8=${_unicode_utf8}
        -DwxUSE_OPENGL=ON
        -DwxUSE_LIBPNG=sys
        -DwxUSE_ZLIB=sys
        -DwxUSE_REGEX=OFF
        -DwxUSE_LIBXPM=builtin
        -DwxUSE_LIBJPEG=sys
        -DwxUSE_LIBTIFF=sys
        -DwxUSE_EXPAT=sys
        -DwxUSE_LIBSDL=OFF
        -DwxUSE_XTEST=OFF
)

if (MSVC)
    add_debug_dep(dep_wxWidgets)
endif ()