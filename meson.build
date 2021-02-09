project('mq2luna', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++2a', 'cpp_rtti=false', 'cpp_eh=none'])


cc = meson.get_compiler('cpp')
lua_dir = meson.current_source_dir() + '/third_party/lua/'
lua_lib = cc.find_library('lua54', dirs : [lua_dir], static : true)

inc_path = include_directories('./include', 'third_party/lua/include')
subdir('src')
