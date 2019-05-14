
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

i18n = import('i18n')

###############################################################################
# OS specific options

on_windows = ['cygwin', 'windows'].contains(host_machine.system())
on_appleos = ['darwin'].contains(host_machine.system())
on_linux = not (on_windows or on_appleos)

static_dependencies = on_windows or on_appleos or get_option('static')

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

config.set('SLIC3R_GUI', get_option('gui'))
if get_option('gui')
  build_arguments += [ '-DSLIC3R_GUI' ]
endif

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
math= cc.find_library('m', required: on_linux)

# Windows only
psapi = cc.find_library('Psapi', required: on_windows)

boost = dependency('boost', version: '>=1.64.0', static: static_dependencies,
  modules: [ 'system', 'filesystem', 'thread', 'log', 'locale', 'regex', ],
)

if on_appleos
  # BOOST_ASIO_DISABLE_KQUEUE : prevents a Boost ASIO bug on OS X:
  # https://svn.boost.org/trac/boost/ticket/5339
  build_arguments += [ '-DBOOST_ASIO_DISABLE_KQUEUE' ]
endif

if on_windows
  build_arguments += [
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_USE_WINAPI_VERSION=0x601',
  ]
endif


if not static_dependencies
  build_arguments += [ '-DBOOST_LOG_DYN_LINK' ]
endif


# Find and configure intel-tbb
tbb = cxx.find_library('tbb',
  has_headers: 'tbb/tbb.h',
  static: static_dependencies,
)

# The Intel TBB library will use the std::exception_ptr feature of C++11.
build_arguments += [ '-DTBB_USE_CAPTURED_EXCEPTION=0' ]

if get_option('buildtype').contains('debug')
  build_arguments += [ '-DTBB_USE_DEBUG=1' ]
endif

if on_windows
  # Suppress implicit linking of the TBB libraries by the Visual Studio compiler.
  build_arguments += [ '-D__TBB_NO_IMPLICIT_LINKAGE' ]
endif


curl = dependency('libcurl', static: static_dependencies)

if static_dependencies
  # libcurl is always linked dynamically to the system libcurl on OSX.
  # On other systems, libcurl is linked statically if SLIC3R_STATIC is set.
  build_arguments += [ '-DCURL_STATICLIB' ]

  if on_linux
    # As of now, our build system produces a statically linked libcurl,
    # which links the OpenSSL library dynamically.
    openssl = dependency('openssl')
  endif
endif


# Find eigen3 or use bundled version
if not static_dependencies
  eigen = dependency('eigen3', required: false)
else
  eigen = declare_dependency(
    include_directories: include_directories('src/eigen')
  )
endif

# Find expat or use bundled version
if (not static_dependencies) # or on_linux
  expat = dependency('expat', required: on_linux)
else
  expat = declare_dependency(
    link_with: static_library('expat',
      'src/expat/xmlparse.c',
      'src/expat/xmlrole.c',
      'src/expat/xmltok.c',
    ),
    include_directories: include_directories('src/expat')
  )
endif



# Find glew or use bundled version
if not static_dependencies
  glew = dependency('glew')
else
  glew = declare_dependency(
    link_with: static_library('glew',
      'src/glew/src/glew.c',
    ),
    include_directories: include_directories('src/glew'),
    compile_args: '-DGLEW_STATIC',
  )
endif


###############################################################################

add_project_arguments(build_arguments, language: ['c', 'cpp'])

subdir('resources/localization')

# Resources install target, configure fhs.hpp on UNIX
if on_windows
  install_subdir('resources',
    install_dir: 'resources',
  )
else
  install_subdir('resources',
    install_dir: get_option('datadir') / meson.project_name(),
  )
endif

subdir('src')

# Perl bindings, currently only used for the unit / integration tests of libslic3r.
# Also runs the unit / integration tests.
#FIXME Port the tests into C++ to finally get rid of the Perl!

if get_option('perl_xs')
#   subdir(xs)
endif

if get_option('sandboxes')
#   subdir(sandboxes)
endif

if get_option('tests')
#   subdir(tests)
endif
