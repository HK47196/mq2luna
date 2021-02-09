/*
 * luna_state.cpp
 * Copyright (C) 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#include "luna_context.hpp"
#include "luna.hpp"
#include "mq2_api.hpp"

LunaContext::LunaContext(const std::string& name) : name{name} {
  main_thread = luaL_newstate();
  luaL_openlibs(main_thread);

  lua_pushstring(main_thread, LUNA_MODULE_KEY);
  lua_pushstring(main_thread, name.c_str());
  lua_rawset(main_thread, LUA_REGISTRYINDEX);

  lua_pushstring(main_thread, LUNA_CTX_PTR_KEY);
  lua_pushlightuserdata(main_thread, this);
  lua_rawset(main_thread, LUA_REGISTRYINDEX);

  pulse_thread = lua_newthread(main_thread);
  event_thread = lua_newthread(main_thread);
  bind_thread = lua_newthread(main_thread);
}

LunaContext::~LunaContext() {
  pulse_thread = nullptr;
  event_thread = nullptr;
  bind_thread = nullptr;
  if (!main_thread) {
    return;
  }
  lua_close(main_thread);
  main_thread = nullptr;
  if (luna == nullptr) {
    return;
  }
  for (auto&& cmd : bound_commands_) {
    luna->remove_bind(cmd);
  }
}

void LunaContext::set_search_path(const char* path) {
  if (path == nullptr) {
    return;
  }
  lua_getglobal(main_thread, "package");
  lua_pushstring(main_thread, path);
  lua_setfield(main_thread, -2, "path");
  lua_pop(main_thread, 1);
}

namespace {
int get_key(lua_State* l, int idx, const char* field_name) {
  if (lua_getfield(l, idx, field_name) == LUA_TFUNCTION) {
    return luaL_ref(l, LUA_REGISTRYINDEX);
  }
  lua_pop(l, 1);
  return LUA_NOREF;
}
} // namespace

bool LunaContext::create_indices() {
  auto ret = lua_getglobal(main_thread, module_global);
  if (ret != LUA_TTABLE) {
    lua_pop(main_thread, 1);
    return false;
  }
  pulse_idx = get_key(main_thread, -1, "pulse");
  zoned_idx = get_key(main_thread, -1, "zoned");
  clean_idx = get_key(main_thread, -1, "clean");
  reload_idx = get_key(main_thread, -1, "reload");
  draw_idx = get_key(main_thread, -1, "draw");
  gamestate_changed_idx = get_key(main_thread, -1, "gamestate_changed");
  write_chat_idx = get_key(main_thread, -1, "write_chat");
  incoming_chat_idx = get_key(main_thread, -1, "incoming_chat");
  begin_zone_idx = get_key(main_thread, -1, "begin_zone");
  end_zone_idx = get_key(main_thread, -1, "end_zone");
  lua_pop(main_thread, 1);
  return true;
}

const char* LunaContext::get_context_name(lua_State* ls) {
  lua_getfield(ls, LUA_REGISTRYINDEX, "luna_module_name");
  const char* module_name = lua_tostring(ls, -1);
  lua_pop(ls, 1);
  return module_name;
}
