/*
 * luna_state.hpp Copyright © 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#ifndef LUNA_STATE_HPP61451
#define LUNA_STATE_HPP61451

#include "lua.hpp"
#include "luna_defs.hpp"
#include <algorithm>
#include <cstdint>
#include <regex>
#include <string>
#include <vector>

constexpr const char* module_global = "luna_module";
struct LunaContext {
  // If this is ever allowed need to add functions to update the stored ctx ptr in the registry
  LunaContext(const LunaContext& other) = delete;
  LunaContext(LunaContext&& other) = delete;
  LunaContext& operator=(const LunaContext& other) = delete;
  const LunaContext& operator=(LunaContext&& other) = delete;

  std::string name;
  lua_State* main_thread;
  lua_State* pulse_thread;
  lua_State* event_thread;
  lua_State* bind_thread;
  bool pulse_yielding = false;
  bool paused = false;

  int pulse_idx = LUA_NOREF;
  int zoned_idx = LUA_NOREF;
  int clean_idx = LUA_NOREF;
  int reload_idx = LUA_NOREF;
  int draw_idx = LUA_NOREF;
  int gamestate_changed_idx = LUA_NOREF;
  int write_chat_idx = LUA_NOREF;
  int incoming_chat_idx = LUA_NOREF;
  int begin_zone_idx = LUA_NOREF;
  int end_zone_idx = LUA_NOREF;

  std::vector<std::string> bound_commands_;

  std::vector<std::regex> event_regexes_;
  std::vector<int> event_fn_keys_;

  std::int64_t sleep_ms = 0;

  LunaContext(const std::string& name);
  ~LunaContext();
  void set_search_path(const char* path);
  bool create_indices();
  static const char* get_context_name(lua_State* ls);
};

#endif /* !LUNA_STATE_HPP61451 */
