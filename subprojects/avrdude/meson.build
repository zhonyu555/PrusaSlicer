
project('avrdude',
  'c',
  version: '6.3-20160220',
)

###############################################################################
# OS dependency

on_windows = ['cygwin', 'windows'].contains(host_machine.system())

avrdude_sources = [
  'arduino.c',
  'avr.c',
  # 'avrftdi.c',
  # 'avrftdi_tpi.c',
  'avrpart.c',
  'avr910.c',
  'bitbang.c',
  'buspirate.c',
  'butterfly.c',
  'config.c',
  'config_gram.c',
  # 'confwin.c',
  'crc16.c',
  # 'dfu.c',
  'fileio.c',
  # 'flip1.c',
  # 'flip2.c',
  # 'ft245r.c',
  # 'jtagmkI.c',
  # 'jtagmkII.c',
  # 'jtag3.c',
  'lexer.c',
  'linuxgpio.c',
  'lists.c',
  # 'par.c',
  'pgm.c',
  'pgm_type.c',
  'pickit2.c',
  'pindefs.c',
  # 'ppi.c',
  # 'ppiwin.c',
  'safemode.c',
  'ser_avrdoper.c',
  'serbb_posix.c',
  'serbb_win32.c',
  'ser_posix.c',
  'ser_win32.c',
  'stk500.c',
  'stk500generic.c',
  'stk500v2.c',
  'term.c',
  'update.c',
  # 'usbasp.c',
  # 'usb_hidapi.c',
  # 'usb_libusb.c',
  # 'usbtiny.c',
  'wiring.c',

  'main.c',
  'avrdude-slic3r.cpp',
]

if on_windows
  avrdude_sources += [
    'windows/getopt.c',
    'windows/unistd.cpp',
    'windows/utf8.c',
  ]
endif

avrdude_conf_gen = executable('avrdude-conf-gen',
  'conf-generate.cpp',
)
# Config file embedding
avrdude_sources += custom_target('avrdude-slic3r.conf.h',
  input : 'avrdude-slic3r.conf',
  output: 'avrdude-slic3r.conf.h',
  command: [
    avrdude_conf_gen, files('avrdude-slic3r.conf'), 'avrdude_slic3r_conf',
  ],
  capture: true,
)

avrdude_arguments = [
  # To enable various useful macros and functions on Unices
  '-D_BSD_SOURCE',
  '-D_DEFAULT_SOURCE',
]

if on_windows
  avrdude_arguments += [ '-DWIN32NATIVE=1', ]
endif

# So that sources find the getopt.h windows drop-in
avrdude_incdirs = on_windows ? ['windows'] : []

avrdude = static_library('avrdude',
  avrdude_sources,
  c_args:   avrdude_arguments,
  cpp_args: avrdude_arguments,
  include_directories: avrdude_incdirs,
)

avrdude_slic3r = executable('avrdude-slic3r',
  'main-standalone.cpp',
  link_with: avrdude,
)


dependency = declare_dependency(
  link_with: avrdude,
  include_directories: '.',
)
