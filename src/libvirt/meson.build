#include_dir = include_directories('../../include')
lexer_build_dir = meson.source_root() + '/../reader-lexer/build'
lexer_include_dir = include_directories('../../../reader-lexer')
console_include_dir = include_directories('.')

serialcon_libvirt_lib = static_library(
  'serialcon-libvirt',
  sources: ['console.c'],
  include_directories: [
    include_directories('.'), 
    lexer_include_dir, 
    statemachine_include_dir,
    commands_include_dir,
  ],
  link_with: [
    commands_lib,
  ],
  c_args: cargs,
)

executable(
  'libvirt',
  sources: ['main.c'],
  include_directories: [
    include_directories('.'),
    commands_include_dir
  ],
  dependencies: [
    meson.get_compiler('c').find_library('lexer', 
      dirs: lexer_build_dir, required: true),
    meson.get_compiler('c').find_library('pthread', required: true)
  ],
  link_with: [
    serialcon_statemachine_lib, 
    serialcon_libvirt_lib,
  ],
)
