/*
 * luna.cpp
 * Copyright (C) 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#include "luna.hpp"
#include "mq2_api.hpp"
#include "utils.hpp"
#include <windows.h>

#include <cstring>
#include <string_view>

namespace zx {
char scratch_buf[4096] = {'\0'};
}

namespace {
int luna_yield(lua_State* ls) {
  if (!luna->in_pulse()) {
    return luaL_error(ls, "yielding is NOT support on non-pulse threads.");
  }
  int nargs = lua_gettop(ls);
  if (nargs > 0) {
    auto ctx = zx::get_context(ls);
    if (ctx == nullptr) {
      return 0;
    }
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
      ctx->sleep_time = now + min + sec + ms;
    } else if (type == LUA_TNUMBER) {
      int sleep_ms = luaL_checkinteger(ls, 1);
      if (!lua_isinteger(ls, 1)) {
        return 0;
      }
      auto now = std::chrono::steady_clock::now();
      ctx->sleep_time = now + std::chrono::milliseconds(sleep_ms);
    } else {
      return luaL_error(ls, "unknown argument passed to luna_yield.");
    }
  }
  return lua_yield(ls, 0);
}

int luna_do(lua_State* ls) {
  auto cmd = luaL_checkstring(ls, 1);
  if (!cmd) {
    return 0;
  }
  lua_remove(ls, 1);
  mq2->DoCommand(cmd);
  return 0;
}

int luna_data(lua_State* ls) {
  auto cmd = luaL_checkstring(ls, 1);
  if (!cmd) {
    return 0;
  }
  lua_remove(ls, 1);
  MQ2TypeVar result;
  if (!mq2->ParseMQ2DataPortion(cmd, result)) {
    lua_pushnil(ls);
  } else if (result.Type == mq2->pIntType) {
    lua_pushinteger(ls, result.Int);
  } else if (result.Type == mq2->pInt64Type) {
    lua_pushinteger(ls, lua_Integer(result.Int64));
  } else if (result.Type == mq2->pFloatType) {
    lua_pushnumber(ls, result.Float);
  } else if (result.Type == mq2->pDoubleType) {
    lua_pushnumber(ls, result.Double);
  } else if (result.Type == mq2->pStringType) {
    lua_pushstring(ls, (const char*)result.Ptr);
  } else if (result.Type == mq2->pBoolType) {
    lua_pushboolean(ls, (bool)result.DWord);
  } else {
    lua_pushboolean(ls, (bool)result.DWord);
  }
  return 1;
}

int luna_echo(lua_State* ls) {
  auto msg = luaL_checkstring(ls, 1);
  if (!msg) {
    return 0;
  }
  lua_remove(ls, 1);
  mq2->WriteChatColor(msg);
  return 0;
}

int luna_bind(lua_State* ls) { return luna->add_bind(ls); }

int luna_add_event(lua_State* ls) {
  auto ctx = zx::get_context(ls);
  if (ctx != nullptr) {
    ctx->add_event_binding(ls);
  }
  return 0;
}

int luna_add_raw_event(lua_State* ls) {
  // TODO
  return 0;
}

int luna_cur_time(lua_State* ls) {
  auto now = std::chrono::steady_clock::now();
  auto seconds_since_epoch = std::chrono::duration_cast<std::chrono::duration<double>>(now.time_since_epoch()).count();
  lua_pushnumber(ls, seconds_since_epoch);
  return 1;
}

const luaL_Reg luna_lib[] = {
    {"yield", luna_yield},
    {"do_command", luna_do},
    {"data", luna_data},
    {"echo", luna_echo},
    {"bind", luna_bind},
    {"add_event", luna_add_event},
    {"add_raw_event", luna_add_raw_event},
    {"cur_time", luna_cur_time},
    {nullptr, nullptr},
};

} // namespace

Luna::Luna() {
  if (mq2->mq2_dir != nullptr) {
    modules_dir = fs::path{mq2->mq2_dir};
    modules_dir.replace_filename("luna");
  } else {
    LOG("failed to locate the mq2 dir, serious error.");
  }
  load_config();
}

Luna::~Luna() { luna_ctxs_.clear(); }

void Luna::Cmd(const char* cmd) {
  if (cmd == nullptr) {
    return;
  }
  todo_luna_cmds_.emplace_back(std::string{cmd});
}

void Luna::BoundCommand(const char* cmd) { todo_bind_commands_.push_back(std::string{cmd}); }

void Luna::print_info() {
  LOG("Active modules: %d", luna_ctxs_.size());
  for (auto&& ls : luna_ctxs_) {
    LOG("=====================");
    LOG("Name: %s", ls->name.c_str());
    LOG("Paused: %s", ls->paused ? "true" : "false");
    // TODO
    LOG(" Main thread stack size: %d", lua_gettop(ls->main_thread));
    LOG(" Pulse thread stack size: %d", lua_gettop(ls->main_thread));
    LOG(" Event thread stack size: %d", lua_gettop(ls->main_thread));
    LOG(" Bind thread stack size: %d", lua_gettop(ls->main_thread));
    LOG("Memory usage: NYI");
    LOG("Current line: NYI");
  }
}

void Luna::print_help() {
  LOG("Usage: /luna {run|stop|pause} {module_name|all}");
  LOG("Usage: /luna {info|help|list}");
}

void Luna::list_available_modules() {
  int num_modules = 0;
  for (auto& p : fs::directory_iterator(modules_dir)) {
    auto path = p.path();
    if (!fs::is_directory(path)) {
      continue;
    }
    ++num_modules;
    if (fs::exists(path.append("module.lua"))) {
      LOG("   %ls", p.path().filename().c_str());
    }
  }
  if (num_modules == 1) {
    LOG("1 module found.");
  } else {
    LOG("%d modules found.", num_modules);
  }
}

void Luna::run_module(std::string_view sv) {
  auto idx = find_index_of(sv);
  if (idx != -1) {
    LOG("module %s is already running.", sv.data());
    return;
  }
  auto module_dir = modules_dir / sv;
  if (!fs::is_directory(module_dir)) {
    LOG("module %s either doesn't exist or is invalid.", sv.data());
    return;
  }
  auto module_path = module_dir;
  module_path /= "module.lua";
  if (!fs::is_regular_file(module_path)) {
    LOG("module %s requires a module.lua", sv.data());
    return;
  }
  auto ls = std::make_unique<LunaContext>(std::string(sv));
  auto search_path = modules_dir.generic_string();
  std::string lua_search_path =
      module_dir.generic_string() + "?.lua;" + search_path + "/lib/?.lua;" + search_path + "/lib/?/init.lua;";
  ls->set_search_path(lua_search_path.c_str());
  DLOG("adding path %s", module_dir.generic_string().c_str());
  luaL_newlib(ls->main_thread, luna_lib);
  lua_setglobal(ls->main_thread, "luna");
  DLOG("running module path %s", module_path.generic_string().c_str());
  if (luaL_dofile(ls->main_thread, module_path.generic_string().c_str()) != LUA_OK) {
    LOG("error running lua module: %s", lua_tostring(ls->main_thread, -1));
    return;
  }
  if (!lua_istable(ls->main_thread, -1)) {
    LOG("1:error running %s, refer to the examples.", sv.data());
    return;
  }
  lua_setglobal(ls->main_thread, module_global);
  if (!ls->create_indices()) {
    LOG("2:error running %s, refer to the examples.", sv.data());
    return;
  }
  luna_ctxs_.emplace_back(std::move(ls));
}

void Luna::stop_module(std::string_view sv) {
  if (sv == "all") {
    LOG("stopping ALL modules.");
    luna_ctxs_.clear();
    return;
  }
  auto idx = find_index_of(sv);
  if (idx == -1) {
    LOG("module %s isn't running.", sv.data());
    return;
  }
  LOG("Stopping module %s.", luna_ctxs_[idx]->name.c_str());
  luna_ctxs_.erase(luna_ctxs_.begin() + idx);
}

void Luna::pause_module(std::string_view sv) {
  auto idx = find_index_of(sv);
  if (idx == -1) {
    LOG("unable to find module %s.", sv.data());
    return;
  }
  auto& ls = luna_ctxs_[idx];
  if (ls->paused) {
    LOG("Unpausing module %s.", luna_ctxs_[idx]->name.c_str());
    ls->paused = false;
    return;
  }
  LOG("Pausing module %s.", luna_ctxs_[idx]->name.c_str());
  ls->paused = true;
}

int Luna::find_index_of(std::string_view ctx_name) {
  int idx = 0;
  for (auto&& ctx : luna_ctxs_) {
    if (ctx->name == ctx_name) {
      return idx;
    }
    ++idx;
  }
  return -1;
}

void Luna::load_config() {
  auto conf_file = modules_dir / "luna_config.lua";
  auto str_path = conf_file.generic_string();
  if (!fs::is_regular_file(conf_file)) {
    return;
  }
  lua_State* l = luaL_newstate();
  luaL_openlibs(l);
  if (luaL_dofile(l, str_path.c_str()) != 0) {
    LOG("error loading Luna config file: %s", lua_tostring(l, -1));
    return;
  }
  if (lua_getglobal(l, "debug") == LUA_TBOOLEAN) {
    debug_ = lua_toboolean(l, -1);
  }
  lua_close(l);
}

void Luna::save_config() {}

int Luna::add_bind(lua_State* ls) {
  auto cmd = luaL_checkstring(ls, 2);
  if (cmd == nullptr) {
    return 0;
  }
  // hackery, check if the command is already bound by something else.
  for (const auto& ctx : luna_ctxs_) {
    if (ctx->has_command_binding(cmd)) {
      return luaL_error(ls, "conflicting bind %s already exists", cmd);
    }
  }
  auto ctx = zx::get_context(ls);
  if (ctx == nullptr) {
    return 0;
  }
  ctx->add_command_binding(ls);
  return 0;
}

void Luna::cleanup_exiting_contexts() {
  for (auto i = 0u; i < luna_ctxs_.size(); ++i) {
    const std::unique_ptr<LunaContext>& ctx = luna_ctxs_[i];
    if (!ctx->exiting) {
      continue;
    }
    luna_ctxs_.erase(luna_ctxs_.begin() + i);
    --i;
  }
}

Luna* luna;
