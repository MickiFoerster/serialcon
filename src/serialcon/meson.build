serialcon_lib = static_library(
  'serialcon',
  sources: ['serialcon.c'],
  include_directories: [
    serialcon_include_dir, 
    console_include_dir,
  ],
  c_args: cargs,
)

executable(
  'serialcon',
  sources: ['main.c', 'getopt_long.c'],
  include_directories: [
    include_directories('.'),
    serialcon_include_dir,
  ],
  link_with: [
    serialcon_lib,
    commands_lib,
    serialcon_libvirt_lib,
    serialcon_statemachine_lib,
  ],
  dependencies: [
    meson.get_compiler('c').find_library('lexer', 
      dirs: lexer_build_dir, required: true),
    meson.get_compiler('c').find_library('pthread', required: true),
  ],
)
