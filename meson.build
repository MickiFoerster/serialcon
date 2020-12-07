project(
    'serialcon', 
    'c',
)

cargs = ['-std=c11', '-Wall', '-Werror', '-ggdb3']

lexer_build_dir = meson.source_root() + '/../reader-lexer/build'
lexer_include_dir = include_directories('../reader-lexer')
serialcon_include_dir = include_directories('include')

subdir('src')
