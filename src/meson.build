# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_MQ2LUNA']

luna_src = [
  'luna.cpp',
  'mq2_api.cpp',
  'plugin_api.cpp',
  'luna_context.cpp',
  'luna_events.cpp',
  'utils.cpp',
]

luna_lib = shared_library('mq2luna', luna_src,
  cpp_args : lib_args,
  gnu_symbol_visibility : 'hidden',
  include_directories : inc_path,
  dependencies : lua_lib,
)
