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
  exit_fn();
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
  pulse_key = get_key(main_thread, -1, "pulse");
  zoned_key = get_key(main_thread, -1, "zoned");
  clean_key = get_key(main_thread, -1, "clean");
  reload_key = get_key(main_thread, -1, "reload");
  draw_key = get_key(main_thread, -1, "draw");
  gamestate_changed_key = get_key(main_thread, -1, "gamestate_changed");
  write_chat_key = get_key(main_thread, -1, "write_chat");
  incoming_chat_key = get_key(main_thread, -1, "incoming_chat");
  begin_zone_key = get_key(main_thread, -1, "begin_zone");
  end_zone_key = get_key(main_thread, -1, "end_zone");
  exit_fn_key = get_key(main_thread, -1, "at_exit");
  lua_pop(main_thread, 1);
  return true;
}

const char* LunaContext::get_context_name(lua_State* ls) {
  lua_getfield(ls, LUA_REGISTRYINDEX, "luna_module_name");
  const char* module_name = lua_tostring(ls, -1);
  lua_pop(ls, 1);
  return module_name;
}

void LunaContext::add_command_binding(lua_State* ls) {
  if (!lua_isfunction(ls, 1)) {
    luaL_checktype(ls, 2, LUA_TFUNCTION);
    return;
  }
  auto cmd = luaL_checkstring(ls, 2);
  if (cmd == nullptr) {
    return;
  }
  DLOG("attempting to add bind command %s", cmd);
  std::string_view sv{cmd};
  if (sv.size() <= 3) {
    luaL_error(ls, "bind command provided was too short.");
    return;
  }
  if (!isalpha(sv[0])) {
    luaL_error(ls, "first character of bind command must be a-Z");
    return;
  }
  for (char c : sv) {
    if (!isalnum(c) && c != '_') {
      luaL_error(ls, "bind command name may only contain a-Z,0-9,_");
      return;
    }
  }

  // makes sure the function is at the top of the stack
  lua_pushvalue(ls, 1);
  // place the function in the registry
  if (!lua_isfunction(ls, -1)) {
    LOG("stack error in add_command_binding!");
    return;
  }
  auto key = luaL_ref(ls, LUA_REGISTRYINDEX);
  bound_command_map_[std::string{sv}] = key;
  DLOG("added bind command %s", cmd);
  return;
}

void LunaContext::add_event_binding(lua_State* ls) {
  if (!lua_isfunction(ls, 1)) {
    luaL_checktype(ls, 1, LUA_TFUNCTION);
    return;
  }
  auto event_str = luaL_checkstring(ls, 2);
  if (event_str == nullptr) {
    return;
  }
  std::string_view event_sv{event_str};
  bool needs_interp = false;
  if (event_sv.find("$[") != std::string_view::npos) {
    // TODO
    luaL_error(ls, "MQ data interpreter for event strings is NYI.");
    return;
  }
  // makes sure the function is at the top of the stack
  lua_pushvalue(ls, 1);
  // place the function in the registry
  if (!lua_isfunction(ls, -1)) {
    LOG("stack error in add_event_bindind!");
    return;
  }

  DLOG("adding event |%s|", event_str);
  if (needs_interp) {
    // TODO
    luaL_error(ls, "MQ data interpreter for event strings is NYI.");
    return;
  } else {
    event_regexes_.emplace_back(std::regex{std::string{event_sv}});
  }
  auto key = luaL_ref(ls, LUA_REGISTRYINDEX);
  event_fn_keys_.push_back(key);
}

bool LunaContext::has_command_binding(std::string_view command) { return bound_command_map_.contains(command); }

void LunaContext::do_command_bind(std::vector<std::string_view> args) {
  if (exiting) {
    return;
  }
  auto cmd = args[0];
  auto it = bound_command_map_.find(cmd);
  if (it == bound_command_map_.end()) {
    LOG("1:ERROR in do_command_bind, please report.");
    return;
  }
  int registry_key = it->second;
  if (registry_key == LUA_NOREF) {
    LOG("2:ERROR in do_command_bind, please report.");
    return;
  }
  auto type = lua_rawgeti(bind_thread, LUA_REGISTRYINDEX, registry_key);
  if (type != LUA_TFUNCTION) {
    // create temp allocation for error string
    std::string cmd_name{cmd};
    LOG("ERROR: can't find bind function for command %s.", cmd_name.c_str());
    lua_pop(bind_thread, 1);
    return;
  }
  char buf[2048] = {'\0'};
  // push the args for the function onto the stack
  int nargs = 0;
  for (auto l = 1u; l < args.size(); ++l) {
    auto&& arg = args[l];
    if (arg.size() == 0 || arg == " ") {
      continue;
    }
    std::strncpy(buf, arg.data(), arg.size());
    buf[arg.size()] = '\0';
    lua_pushstring(bind_thread, buf);
    ++nargs;
  }
  in_bind_call_ = true;
  if (lua_pcall(bind_thread, nargs, 0, 0) != LUA_OK) {
    const char* event_msg = lua_tostring(bind_thread, -1);
    LOG("\ardo_command_bind had an error!");
    if (event_msg != nullptr) {
      LOG("  \arerror message: %s", event_msg);
      lua_pop(bind_thread, 1);
    }
  }
  in_bind_call_ = false;
}

void LunaContext::do_event(const std::string& event_line) {
  if (exiting) {
    return;
  }
  std::smatch sm;
  for (auto i = 0u; i < event_regexes_.size(); ++i) {
    auto&& re = event_regexes_[i];
    if (!std::regex_match(event_line, sm, re)) {
      continue;
    }
    int nargs = sm.size() - 1;
    // push the event handler function onto the stack.
    auto registry_key = event_fn_keys_[i];
    auto type = lua_rawgeti(event_thread, LUA_REGISTRYINDEX, registry_key);
    if (type != LUA_TFUNCTION) {
      // TODO: error handling if fn not found
      lua_pop(event_thread, 1);
      continue;
    }
    // 0 = full match string
    for (auto l = 1u; l < sm.size(); ++l) {
      auto match_len = sm.length(l);
      auto match_offset = sm.position(l);
      lua_pushlstring(event_thread, event_line.data() + match_offset, match_len);
    }
    in_event_call_ = true;
    if (lua_pcall(event_thread, nargs, 0, 0) != LUA_OK) {
      const char* event_msg = lua_tostring(event_thread, -1);
      LOG("event matching |%s| had an error.", event_line.c_str());
      if (event_msg != nullptr) {
        LOG("  \arerror message: %s", event_msg);
        lua_pop(event_thread, 1);
      }
    }
    in_event_call_ = false;
  }
}

bool LunaContext::matches_event(const char* event_line) {
  for (const std::regex& re : event_regexes_) {
    if (std::regex_match(event_line, re)) {
      return true;
    }
  }
  return false;
}

void LunaContext::pulse() {
  if (exiting) {
    DLOG("Attempted to call pulse in exiting content.")
    return;
  }
  if (paused || pulse_key == LUA_NOREF) {
    return;
  }
  auto now = std::chrono::steady_clock::now();
  if (sleep_time > now) {
    return;
  }
  if (!pulse_yielding) {
    // push the pulse function onto the stack if it's not a yielded thread
    auto type = lua_rawgeti(pulse_thread, LUA_REGISTRYINDEX, pulse_key);
    if (type != LUA_TFUNCTION) {
      LOG("1:UNKNOWN ERROR IN PULSE.");
      exiting = true;
      return;
    }
  }
  int nargs;
  auto ret = lua_resume(pulse_thread, nullptr, 0, &nargs);
  switch (ret) {
  case LUA_OK:
    pulse_yielding = false;
    break;
  case LUA_YIELD:
    pulse_yielding = true;
    break;
  case LUA_ERRRUN:
  case LUA_ERRMEM:
  case LUA_ERRSYNTAX:
  case LUA_ERRFILE:
    LOG("Received a lua error, stopping module %s.", name.c_str());
    // TODO: full backtrace/stack dump
    LOG("lua err string: %s", lua_tostring(pulse_thread, -1));
    exiting = true;
    return;
  default:
    LOG("2:UNKNOWN ERROR IN PULSE.");
    exiting = true;
    return;
  }
  if (nargs != 0) {
    // TODO
    LOG("FIXME NARGS");
  }
}

void LunaContext::zoned() { call_registry_fn(zoned_key, "zoned", event_thread); }
void LunaContext::reload_ui() { call_registry_fn(reload_key, "reload_ui", event_thread); }
void LunaContext::draw_hud() { call_registry_fn(draw_key, "draw_hud", event_thread); }
void LunaContext::set_game_state(GameState) {
  call_registry_fn(gamestate_changed_key, "gamestate_changed", event_thread);
}

void LunaContext::call_registry_fn(int key, const char* fn_name, lua_State* thread) {
  if (exiting || key == LUA_NOREF) {
    return;
  }
  auto type = lua_rawgeti(thread, LUA_REGISTRYINDEX, key);
  if (type != LUA_TFUNCTION) {
    LOG("\ar%s key is set, but not a function? Please report!", fn_name);
    return;
  }
  if (lua_pcall(thread, 0, 0, 0) != LUA_OK) {
    const char* event_msg = lua_tostring(thread, -1);
    LOG("\ar%s handler had an error!", fn_name);
    if (event_msg != nullptr) {
      LOG("  \arerror message: %s", event_msg);
      lua_pop(thread, 1);
    }
  }
}

void LunaContext::exit_fn() {
  if (did_exit_) {
    return;
  }
  did_exit_ = true;

  if (exit_fn_key == LUA_NOREF) {
    return;
  }
  auto type = lua_rawgeti(main_thread, LUA_REGISTRYINDEX, exit_fn_key);
  if (type != LUA_TFUNCTION) {
    LOG("\arexit_fn key is set, but not a function? Please report!");
    return;
  }
  if (lua_pcall(main_thread, 0, 0, 0) != LUA_OK) {
    const char* event_msg = lua_tostring(main_thread, -1);
    LOG("\arat_exit handler had an error!");
    if (event_msg != nullptr) {
      LOG("  \arerror message: %s", event_msg);
      lua_pop(main_thread, 1);
    }
  }
}
