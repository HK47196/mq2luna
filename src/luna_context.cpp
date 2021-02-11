/*
 * luna_state.cpp
 * Copyright (C) 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#include "luna_context.hpp"
#include "luna.hpp"
#include "mq2_api.hpp"

namespace {
int get_key(lua_State* l, int idx, const char* field_name) {
  if (lua_getfield(l, idx, field_name) == LUA_TFUNCTION) {
    return luaL_ref(l, LUA_REGISTRYINDEX);
  }
  lua_pop(l, 1);
  return LUA_NOREF;
}

EventKeys get_event_keys(lua_State* main_state) {
  auto ret = lua_getglobal(main_state, module_global);
  if (ret != LUA_TTABLE) {
    lua_pop(main_state, 1);
    return {};
  }
  const EventKeys ek = {
      .pulse = get_key(main_state, -1, "pulse"),
      .zoned = get_key(main_state, -1, "zoned"),
      .clean = get_key(main_state, -1, "clean"),
      .reload = get_key(main_state, -1, "reload"),
      .draw = get_key(main_state, -1, "draw"),
      .gamestate_changed = get_key(main_state, -1, "gamestate_changed"),
      .write_chat = get_key(main_state, -1, "write_chat"),
      .incoming_chat = get_key(main_state, -1, "incoming_chat"),
      .begin_zone = get_key(main_state, -1, "begin_zone"),
      .end_zone = get_key(main_state, -1, "end_zone"),
      .exit_fn = get_key(main_state, -1, "at_exit"),
  };
  lua_pop(main_state, 1);
  return ek;
}
} // namespace

LunaContext::LunaContext(const std::string& name) : name{name} {
  threads_.main = luaL_newstate();
  luaL_openlibs(threads_.main);

  lua_pushstring(threads_.main, LUNA_MODULE_KEY);
  lua_pushstring(threads_.main, name.c_str());
  lua_rawset(threads_.main, LUA_REGISTRYINDEX);

  lua_pushstring(threads_.main, LUNA_CTX_PTR_KEY);
  lua_pushlightuserdata(threads_.main, this);
  lua_rawset(threads_.main, LUA_REGISTRYINDEX);

  threads_.pulse = lua_newthread(threads_.main);
  threads_.event = lua_newthread(threads_.main);
  threads_.bind = lua_newthread(threads_.main);
}

LunaContext::~LunaContext() {
  exit_fn();
  threads_.pulse = nullptr;
  threads_.event = nullptr;
  threads_.bind = nullptr;
  if (!threads_.main) {
    return;
  }
  lua_close(threads_.main);
  threads_.main = nullptr;
  if (luna == nullptr) {
    return;
  }
}

void LunaContext::set_search_path(const char* path) {
  if (path == nullptr) {
    return;
  }
  lua_getglobal(threads_.main, "package");
  lua_pushstring(threads_.main, path);
  lua_setfield(threads_.main, -2, "path");
  lua_pop(threads_.main, 1);
}

bool LunaContext::create_indices() {
  auto ret = lua_getglobal(threads_.main, module_global);
  if (ret != LUA_TTABLE) {
    lua_pop(threads_.main, 1);
    return false;
  }
  keys_ = get_event_keys(threads_.main);
  lua_pop(threads_.main, 1);
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

bool LunaContext::has_command_binding(std::string_view command) const { return bound_command_map_.contains(command); }

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
  auto type = lua_rawgeti(threads_.bind, LUA_REGISTRYINDEX, registry_key);
  if (type != LUA_TFUNCTION) {
    // create temp allocation for error string
    std::string cmd_name{cmd};
    LOG("ERROR: can't find bind function for command %s.", cmd_name.c_str());
    lua_pop(threads_.bind, 1);
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
    lua_pushstring(threads_.bind, buf);
    ++nargs;
  }
  if (lua_pcall(threads_.bind, nargs, 0, 0) != LUA_OK) {
    const char* event_msg = lua_tostring(threads_.bind, -1);
    LOG("\ardo_command_bind had an error!");
    if (event_msg != nullptr) {
      LOG("  \arerror message: %s", event_msg);
      lua_pop(threads_.bind, 1);
    }
  }
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
    auto type = lua_rawgeti(threads_.event, LUA_REGISTRYINDEX, registry_key);
    if (type != LUA_TFUNCTION) {
      // TODO: error handling if fn not found
      lua_pop(threads_.event, 1);
      continue;
    }
    // 0 = full match string
    for (auto l = 1u; l < sm.size(); ++l) {
      auto match_len = sm.length(l);
      auto match_offset = sm.position(l);
      lua_pushlstring(threads_.event, event_line.data() + match_offset, match_len);
    }
    if (lua_pcall(threads_.event, nargs, 0, 0) != LUA_OK) {
      const char* event_msg = lua_tostring(threads_.event, -1);
      LOG("event matching |%s| had an error.", event_line.c_str());
      if (event_msg != nullptr) {
        LOG("  \arerror message: %s", event_msg);
        lua_pop(threads_.event, 1);
      }
    }
  }
}

bool LunaContext::matches_event(const char* event_line) const {
  for (const std::regex& re : event_regexes_) {
    if (std::regex_match(event_line, re)) {
      return true;
    }
  }
  return false;
}

int LunaContext::yield_event(lua_State* ls) {
  int nargs = lua_gettop(ls);
  if (nargs > 0) {
    int type = lua_type(ls, 1);
    if (type == LUA_TTABLE) {
      lua_getfield(ls, 1, "min");
      auto min = std::chrono::minutes(lua_tointeger(ls, -1));
      lua_getfield(ls, 1, "sec");
      auto sec = std::chrono::seconds(lua_tointeger(ls, -1));
      lua_getfield(ls, 1, "ms");
      auto ms = std::chrono::milliseconds(lua_tointeger(ls, -1));
      lua_pop(ls, 3);
      auto now = std::chrono::steady_clock::now();
      sleep_time = now + min + sec + ms;
    } else if (type == LUA_TNUMBER) {
      int sleep_ms = luaL_checkinteger(ls, 1);
      if (!lua_isinteger(ls, 1)) {
        return 0;
      }
      auto now = std::chrono::steady_clock::now();
      sleep_time = now + std::chrono::milliseconds(sleep_ms);
      lua_pop(ls, 1);
    } else {
      return luaL_error(ls, "unknown argument passed to luna.yield with type %s. Nargs: %d", luaL_typename(ls, 1), nargs);
    }
  }
  return lua_yield(ls, 0);
}

void LunaContext::pulse() {
  if (exiting) {
    DLOG("Attempted to call pulse in exiting content.")
    return;
  }
  if (paused || keys_.pulse == LUA_NOREF) {
    return;
  }
  auto now = std::chrono::steady_clock::now();
  if (sleep_time > now) {
    return;
  }
  if (!pulse_yielding) {
    // push the pulse function onto the stack if it's not a yielded thread
    auto type = lua_rawgeti(threads_.pulse, LUA_REGISTRYINDEX, keys_.pulse);
    if (type != LUA_TFUNCTION) {
      LOG("1:UNKNOWN ERROR IN PULSE.");
      exiting = true;
      return;
    }
  }
  int nargs;
  auto ret = lua_resume(threads_.pulse, nullptr, 0, &nargs);
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
    LOG("lua err string: %s", lua_tostring(threads_.pulse, -1));
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

void LunaContext::zoned() { call_registry_fn(keys_.zoned, "zoned", threads_.event); }
void LunaContext::reload_ui() { call_registry_fn(keys_.reload, "reload_ui", threads_.event); }
void LunaContext::draw_hud() { call_registry_fn(keys_.draw, "draw_hud", threads_.event); }
void LunaContext::set_game_state(GameState) {
  call_registry_fn(keys_.gamestate_changed, "gamestate_changed", threads_.event);
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

  if (keys_.exit_fn == LUA_NOREF) {
    return;
  }
  auto type = lua_rawgeti(threads_.main, LUA_REGISTRYINDEX, keys_.exit_fn);
  if (type != LUA_TFUNCTION) {
    LOG("\arexit_fn key is set, but not a function? Please report!");
    return;
  }
  if (lua_pcall(threads_.main, 0, 0, 0) != LUA_OK) {
    const char* event_msg = lua_tostring(threads_.main, -1);
    LOG("\arat_exit handler had an error!");
    if (event_msg != nullptr) {
      LOG("  \arerror message: %s", event_msg);
      lua_pop(threads_.main, 1);
    }
  }
}
