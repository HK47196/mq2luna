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
#include <map>
#include <regex>
#include <string>
#include <vector>

constexpr const char* module_global = "luna_module";
struct LunaContext {
  LunaContext(const std::string& name);
  ~LunaContext();
  // If this is ever allowed need to add functions to update the stored ctx ptr in the registry
  LunaContext(const LunaContext& other) = delete;
  LunaContext(LunaContext&& other) = delete;
  LunaContext& operator=(const LunaContext& other) = delete;
  const LunaContext& operator=(LunaContext&& other) = delete;

  void add_command_binding(lua_State* ls);
  void add_event_binding(lua_State* ls);
  bool has_command_binding(std::string_view command);
  void do_command_bind(std::vector<std::string_view> args);
  void do_event(const std::string& event_line);
  bool matches_event(const char* event_line);

  std::string name;
  lua_State* main_thread;
  lua_State* pulse_thread;
  lua_State* event_thread;
  lua_State* bind_thread;
  bool pulse_yielding = false;
  bool paused = false;

  std::chrono::steady_clock::time_point sleep_time;

  bool exiting = false;

  void set_search_path(const char* path);
  bool create_indices();
  static const char* get_context_name(lua_State* ls);

  void pulse();
  void zoned();
  void reload_ui();
  void draw_hud();
  void set_game_state(GameState game_state);

private:
  bool in_bind_call_ = false;
  bool in_event_call_ = false;

  std::vector<std::regex> event_regexes_;
  std::vector<int> event_fn_keys_;
  std::map<std::string, int, std::less<>> bound_command_map_;
  int pulse_key = LUA_NOREF;
  int zoned_key = LUA_NOREF;
  int clean_key = LUA_NOREF;
  int reload_key = LUA_NOREF;
  int draw_key = LUA_NOREF;
  int gamestate_changed_key = LUA_NOREF;
  int write_chat_key = LUA_NOREF;
  int incoming_chat_key = LUA_NOREF;
  int begin_zone_key = LUA_NOREF;
  int end_zone_key = LUA_NOREF;
  int exit_fn_key = LUA_NOREF;

  void call_registry_fn(int key, const char* fn_name, lua_State* thread);

  void exit_fn();
  bool did_exit_ = false;
};

#endif /* !LUNA_STATE_HPP61451 */
