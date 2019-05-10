
project('Slic3r',
   'c', 'cpp',
  version: '1.0',

  default_options: [
    'c_std=gnu11',
    'cpp_std=gnu++11',
  ],
)

cc = meson.get_compiler('c')
cxx= meson.get_compiler('cpp')
config = configuration_data()


###############################################################################
# OS specific options

on_windows = ['cygwin', 'windows'].contains(host_machine.system())
on_appleos = ['darwin'].contains(host_machine.system())
on_linux = not (on_windows or on_appleos)

needs_static_dependencies = on_windows or on_appleos

build_arguments = [
  '-DwxUSE_UNICODE',
  '-D_UNICODE',
  '-DWXINTL_NO_GETTEXT_MACRO',
]

if on_windows
  windows_sdk_path = get_option('windows_sdk_path')
  include_test = windows_sdk_path / 'include/winrt/windows.graphics.printing3d.h'

  build_arguments += [
    '-D_CRT_SECURE_NO_WARNINGS',
    '-D_SCL_SECURE_NO_WARNINGS',
    '-D_USE_MATH_DEFINES',
    '-D_WIN32',
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_USE_WINAPI_VERSION=0x601',
  ]
endif

if on_appleos
  build_arguments += cxx.get_supported_arguments([
    '-Werror=partial-availability',
    '-Werror=unguarded-availability',
    '-Werror=unguarded-availability-new',
  ])
endif

build_arguments += cxx.get_supported_arguments([
  '-fext-numeric-literals',
  '-Wno-reorder',
  '-Werror=return-type',
  '-Wno-ignored-attributes',
])

###############################################################################
# Project options


static_dependencies = get_option('static') or needs_static_dependencies


config.set('SLIC3R_GUI', get_option('gui'))

# TODO Visual studio parallel implemented by Meson ?
# add_compile_options(-bigobj -Zm316)

if get_option('profile')
  message('Slic3r will be built with a Shiny invasive profiler')
  build_arguments += [ '-DSLIC3R_PROFILE' ]
endif


###############################################################################
# Dependencies

# Automatic detection
threads = dependency('threads')
math_lib= cc.find_library('m', required : false)


###############################################################################

add_project_arguments(build_arguments, language: ['c', 'cpp'])

rootInclude = include_directories('.')





# Find and configure boost
if(SLIC3R_STATIC)
    # Use static boost libraries.
    set(Boost_USE_STATIC_LIBS ON)
    # Use boost libraries linked statically to the C++ runtime.
    # set(Boost_USE_STATIC_RUNTIME ON)
endif()
#set(Boost_DEBUG ON)
# set(Boost_COMPILER "-vc120")
if(NOT WIN32)
    # boost::process was introduced first in version 1.64.0
    set(MINIMUM_BOOST_VERSION "1.64.0")
endif()
find_package(Boost ${MINIMUM_BOOST_VERSION} REQUIRED COMPONENTS system filesystem thread log locale regex)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    if (APPLE)
        # BOOST_ASIO_DISABLE_KQUEUE : prevents a Boost ASIO bug on OS X: https://svn.boost.org/trac/boost/ticket/5339
        add_definitions(-DBOOST_ASIO_DISABLE_KQUEUE)
    endif()
    if(NOT SLIC3R_STATIC)
        add_definitions(-DBOOST_LOG_DYN_LINK)
    endif()
endif()

# Find and configure intel-tbb
if(SLIC3R_STATIC)
    set(TBB_STATIC 1)
endif()
set(TBB_DEBUG 1)
find_package(TBB REQUIRED)
include_directories(${TBB_INCLUDE_DIRS})
add_definitions(${TBB_DEFINITIONS})
if(MSVC)
    # Suppress implicit linking of the TBB libraries by the Visual Studio compiler.
    add_definitions(-D__TBB_NO_IMPLICIT_LINKAGE)
endif()
# The Intel TBB library will use the std::exception_ptr feature of C++11.
add_definitions(-DTBB_USE_CAPTURED_EXCEPTION=0)

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

if (SLIC3R_STATIC)
    if (NOT APPLE)
        # libcurl is always linked dynamically to the system libcurl on OSX.
        # On other systems, libcurl is linked statically if SLIC3R_STATIC is set.
        add_definitions(-DCURL_STATICLIB)
    endif()
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # As of now, our build system produces a statically linked libcurl,
        # which links the OpenSSL library dynamically.
        find_package(OpenSSL REQUIRED)
        message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
        message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
        include_directories(${OPENSSL_INCLUDE_DIR})
    endif()
endif()

## OPTIONAL packages

# Find eigen3 or use bundled version
if (NOT SLIC3R_STATIC)
    find_package(Eigen3 3)
endif ()
if (NOT Eigen3_FOUND)
    set(Eigen3_FOUND 1)
    set(EIGEN3_INCLUDE_DIR ${LIBDIR}/eigen/)
endif ()
include_directories(${EIGEN3_INCLUDE_DIR})

# Find expat or use bundled version
# Always use the system libexpat on Linux.
if (NOT SLIC3R_STATIC OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(EXPAT)
endif ()
if (NOT EXPAT_FOUND)
    add_library(expat STATIC
        ${LIBDIR}/expat/xmlparse.c
        ${LIBDIR}/expat/xmlrole.c
        ${LIBDIR}/expat/xmltok.c
    )
    set(EXPAT_FOUND 1)
    set(EXPAT_INCLUDE_DIRS ${LIBDIR}/expat/)
    set(EXPAT_LIBRARIES expat)
endif ()
include_directories(${EXPAT_INCLUDE_DIRS})

# Find glew or use bundled version
if (NOT SLIC3R_STATIC)
    find_package(GLEW)
endif ()
if (NOT GLEW_FOUND)
    add_library(glew STATIC ${LIBDIR}/glew/src/glew.c)
    set(GLEW_FOUND 1)
    set(GLEW_INCLUDE_DIRS ${LIBDIR}/glew/include/)
    set(GLEW_LIBRARIES glew)
    add_definitions(-DGLEW_STATIC)
endif ()
include_directories(${GLEW_INCLUDE_DIRS})

# l10n
set(L10N_DIR "${SLIC3R_RESOURCES_DIR}/localization")
add_custom_target(pot
    COMMAND xgettext --keyword=L --add-comments=TRN --from-code=UTF-8 --debug
        -f "${L10N_DIR}/list.txt"
        -o "${L10N_DIR}/Slic3rPE.pot"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Generate pot file from strings in the source tree"
)

# Resources install target, configure fhs.hpp on UNIX
if (WIN32)
    install(DIRECTORY "${SLIC3R_RESOURCES_DIR}/" DESTINATION "${CMAKE_INSTALL_PREFIX}/resources")
else ()
    set(SLIC3R_FHS_RESOURCES "${CMAKE_INSTALL_FULL_DATAROOTDIR}/slic3r-prusa3d")
    install(DIRECTORY "${SLIC3R_RESOURCES_DIR}/" DESTINATION "${SLIC3R_FHS_RESOURCES}")
endif ()


configure_file(
  input : 'src/platform/unix/fhs.hpp.in',
  output: 'src/platform/unix/fhs.hpp',
  configuration: config,
)


subdir('src')

# Perl bindings, currently only used for the unit / integration tests of libslic3r.
# Also runs the unit / integration tests.
#FIXME Port the tests into C++ to finally get rid of the Perl!
if get_option('perl_xs')
  subdir(xs)
endif

if get_option('sandboxes')
  subdir(sandboxes)
endif

if get_option('tests')
  subdir(tests)
endif()
