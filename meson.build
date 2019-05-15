project('Slic3r',
   'c', 'cpp',
  version: '1.42.0',

  default_options: [
    'c_std=gnu11',
    'cpp_std=gnu++11',
  ],
)

###############################################################################
# Version Info

project_name_pretty   = 'Slic3r Prusa Edition'
project_name_key      = 'Slic3rPE'
project_version_full  = '1.42.0-beta2'

# TODO add commit info
project_build         = project_version_full + '+UNKNOWN'
project_build_id      = project_version_full

project_version_rcdots= meson.project_version()
project_version_rc    = ','.join(project_version_rcdots.split('.'))


###############################################################################

cc = meson.get_compiler('c')
cxx= meson.get_compiler('cpp')
config = configuration_data()

i18n = import('i18n')

build_arguments = []

###############################################################################
# OS specific options

on_windows = ['cygwin', 'windows'].contains(host_machine.system())
on_appleos = ['darwin'].contains(host_machine.system())
on_linux = not (on_windows or on_appleos)

static_dependencies = on_windows or on_appleos or get_option('static')

if on_windows
  windows_sdk_path = get_option('windows_sdk_path')
  include_test = windows_sdk_path/'include/winrt/windows.graphics.printing3d.h'

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

if on_linux
  config.set('SLIC3R_FHS', get_option('fhs'))
  standard_datadir = get_option('datadir') / meson.project_name()
  config.set('SLIC3R_FHS_RESOURCES', get_option('prefix') / standard_datadir)
endif

build_arguments += cxx.get_supported_arguments([
  '-fext-numeric-literals',
  '-Wno-reorder',
  '-Werror=return-type',
  '-Wno-ignored-attributes',
])

###############################################################################
# Project options

config.set('SLIC3R_APP_NAME',       project_name_pretty)
config.set('SLIC3R_APP_KEY',        project_name_key)
config.set('SLIC3R_VERSION',        project_version_full)
config.set('SLIC3R_BUILD',          project_build)
config.set('SLIC3R_BUILD_ID',       project_build_id)
config.set('SLIC3R_RC_VERSION',     project_version_rcdots)
config.set('SLIC3R_RC_VERSION_DOTS',project_version_rc)


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
# Linux only
math    = cc.find_library('m', required: on_linux)
# Windows only
psapi   = cc.find_library('Psapi', required: on_windows)


# Boost configuration
boost = dependency('boost', version: '>=1.64.0', static: static_dependencies,
  modules: [ 'system', 'filesystem', 'thread', 'log', 'locale', 'regex', ],
)

if not static_dependencies
  build_arguments += [ '-DBOOST_LOG_DYN_LINK' ]
endif

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


if get_option('gui')
  # wxWidgets configuration
  wx_version = (on_linux and get_option('wx_stable')) ? '>=3.0' : '>=3.1'

  wx_modules = [
    'adv',
    'base',
    'core',
    'gl',
    'html',
  ]

  if on_linux
    wx_modules += [
      '--toolkit=gtk@0@'.format(get_option('wx_gtk_version')),
      '--unicode=yes',
      '--static=@0@'.format(static_dependencies ? 'yes' : 'no'),
    ]
  endif

  wxwidgets = dependency('WxWidgets',
    modules: wx_modules,
    version: wx_version,
  )

  build_arguments += [
    '-DwxUSE_UNICODE',
    '-D_UNICODE',
    '-DWXINTL_NO_GETTEXT_MACRO',
  ]


  if not on_windows
    zlib = dependency('zlib')
  else
    zlib = dependency('', required: false)
  endif

  # OpenGL configuration
  if on_windows
    gl = [
      cxx.find_library('user32'),
      cxx.find_library('Setupapi'),
      cxx.find_library('OpenGL32'),
      cxx.find_library('GlU32'),
    ]
  elif on_appleos
    gl = [
      declare_dependency(link_args: '-framework OpenGL')
    ]
  elif on_linux
    gl = [
      cxx.find_library('GL'),
      cxx.find_library('GLU'),
    ]
  endif
endif



# Find and configure intel-tbb
tbb = cxx.find_library('tbb',
  has_headers: 'tbb/tbb.h',
  static: static_dependencies,
)

# The Intel TBB library will use the std::exception_ptr feature of C++11.
build_arguments += [ '-DTBB_USE_CAPTURED_EXCEPTION=0' ]

if get_option('buildtype').contains('deb')
  build_arguments += [ '-DTBB_USE_DEBUG=1' ]
endif

# Suppress implicit linking of the TBB libraries by the Visual Studio compiler.
if on_windows
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
    curl = [ curl, dependency('openssl') ]
  endif
endif


###############################################################################
# Specify project-wide arguments
add_project_arguments(build_arguments, language: ['c', 'cpp'])


###############################################################################
# Maybe-bundled libraries

# Find eigen3 or use bundled version
if not static_dependencies
  eigen = dependency('eigen3', required: false)
else
  eigen = declare_dependency(
    include_directories: include_directories('src/eigen')
  )
endif


# Find expat or use bundled version
if (not static_dependencies) or on_linux
  expat = dependency('expat', required: on_linux)
else
  expat = subproject('expat').get_variable('dependency')
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

libnest2d = subproject('libnest2d').get_variable('dependency')

avrdude = subproject('avrdude').get_variable('dependency')

###############################################################################

subdir('resources/localization')
subdir('src')

# Perl bindings, currently only used for the unit / integration tests.
# Also runs the unit / integration tests.
#FIXME Port the tests into C++ to finally get rid of the Perl!
if get_option('perl_xs')
#   subdir(xs)
endif

if get_option('sandboxes')
  subdir(sandboxes)
endif

if get_option('tests')
  subdir(tests)
endif


# Resources install target, configure fhs.hpp on UNIX
if on_windows
  resources_install_dir = 'resources'
else
  resources_install_dir = standard_datadir
endif

install_subdir('resources',
  install_dir: resources_install_dir,
)
