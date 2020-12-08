serialcon_statemachine_lib = static_library(
  'statemachine',
  sources: ['statemachine.c'],
  include_directories: [
    lexer_include_dir, 
    commands_include_dir,
    serialcon_include_dir,
  ],
  dependencies: [
    meson.get_compiler('c').find_library('lexer', 
      dirs: lexer_build_dir, required: true),
  ],
  link_with: [commands_lib],
  c_args: cargs,
)

