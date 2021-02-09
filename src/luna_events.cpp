/*
 * luna_events.cpp
 * Copyright (C) 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#include <cstring>

#include "luna.hpp"
#include "mq2_api.hpp"
#include "utils.hpp"

void Luna::OnZoned() {}

void Luna::OnCleanUI() {}

void Luna::OnReloadUI() {}

void Luna::OnDrawHUD() {}

void Luna::SetGameState(GameState game_state) {}

void Luna::do_binds() {
  // index-based loop because it may be possible that more binds are added
  // during.
  for (auto i = 0u; i < todo_bind_commands_.size(); ++i) {
    auto cmd_str = todo_bind_commands_[i];
    DLOG("processing %s", cmd_str.c_str());
    auto sv = std::string_view{cmd_str};
    // remove leading spaces
    sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size()));
    if (sv.size() == 0) {
      continue;
    }
    // split on whitespace
    auto args = zx::strsplit(sv);
    // first cmd is the function name
    auto cmd = args[0];
    auto it = bound_command_map_.find(cmd);
    if (it == bound_command_map_.end()) {
      LOG("No luna command found for '%s'", cmd_str.c_str());
      continue;
    }
    LunaContext* ctx = std::get<0>(it->second);
    int registry_key = std::get<1>(it->second);
    if (ctx == nullptr || registry_key == LUA_NOREF) {
      LOG("ERR");
      bound_command_map_.erase(it);
      continue;
    }
    auto type = lua_rawgeti(ctx->bind_thread, LUA_REGISTRYINDEX, registry_key);
    if (type != LUA_TFUNCTION) {
      LOG("ERROR: can't find bind function for command %s.", cmd_str.c_str());
      bound_command_map_.erase(it);
      lua_pop(ctx->bind_thread, 1);
      continue;
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
      lua_pushstring(ctx->bind_thread, buf);
      ++nargs;
    }
    lua_call(ctx->bind_thread, nargs, 0);
  }
  todo_bind_commands_.clear();
}

void Luna::do_events() {
  std::vector<std::string_view> matches;
  std::smatch cm;
  // index-based loop because it may be possible that more events are added
  // during.
  for (auto k = 0u; k < todo_events_.size(); ++k) {
    const std::string& event_str = todo_events_[k];
    for (auto&& ctx : luna_ctxs_) {
      for (auto i = 0u; i < ctx->event_regexes_.size(); ++i) {
        auto&& re = ctx->event_regexes_[i];
        if (!std::regex_match(event_str, cm, re)) {
          continue;
        }
        int nargs = cm.size() - 1;
        // push the event handler function onto the stack.
        auto registry_key = ctx->event_fn_keys_[i];
        auto type = lua_rawgeti(ctx->event_thread, LUA_REGISTRYINDEX, registry_key);
        if (type != LUA_TFUNCTION) {
          // TODO: error handling if fn not found
          lua_pop(ctx->event_thread, 1);
          continue;
        }
        // 0 = full match string
        for (auto l = 1u; l < cm.size(); ++l) {
          auto match_len = cm.length(l);
          auto match_offset = cm.position(l);
          lua_pushlstring(ctx->event_thread, event_str.data() + match_offset, match_len);
        }
        if (lua_pcall(ctx->event_thread, nargs, 0, 0) != LUA_OK) {
          const char* event_msg = lua_tostring(ctx->event_thread, -1);
          LOG("event matching |%s| had an error.", event_str.c_str());
          if (event_msg != nullptr) {
            LOG("  \arerror message: %s", event_msg);
            lua_pop(ctx->event_thread, 1);
          }
        }
      }
    }
  }
  todo_events_.clear();
}

void Luna::do_luna_commands() {
  for (auto i = 0u; i < todo_luna_cmds_.size(); ++i) {
    std::string_view sv = todo_luna_cmds_[i];
    LOG("handling command: %s", sv.data());
    sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size()));
    if (sv.starts_with("run ")) {
      sv.remove_prefix(4);
      sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size()));
      run_module(sv);
    } else if (sv.starts_with("stop ")) {
      sv.remove_prefix(5);
      sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size()));
      stop_module(sv);
    } else if (sv.starts_with("pause ")) {
      sv.remove_prefix(6);
      sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size()));
      pause_module(sv);
    } else if (sv == "info") {
      print_info();
    } else if (sv == "list") {
      list_available_modules();
    } else if (sv == "help") {
      print_help();
    } else {
      LOG("unrecognized command: %s.", sv.data());
      print_help();
    }
  }
  todo_luna_cmds_.clear();
}

void Luna::OnPulse() {
  do_luna_commands();
  do_events();
  do_binds();
  in_pulse_ = true;

  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_).count();
  last_tick_ = now;

  int nargs;
  for (auto i = 0u; i < luna_ctxs_.size(); ++i) {
    auto&& ctx = luna_ctxs_[i];
    if (ctx->paused || ctx->pulse_idx == LUA_NOREF) {
      continue;
    }
    if (ctx->sleep_ms > 0) {
      ctx->sleep_ms -= elapsed;
      ctx->sleep_ms = std::max(0ll, ctx->sleep_ms);
      continue;
    }
    if (!ctx->pulse_yielding) {
      // push the pulse function onto the stack if it's not a yielded thread
      auto type = lua_rawgeti(ctx->pulse_thread, LUA_REGISTRYINDEX, ctx->pulse_idx);
      if (type != LUA_TFUNCTION) {
        LOG("1:UNKNOWN ERROR IN PULSE.");
        luna_ctxs_.erase(luna_ctxs_.begin() + i);
        --i;
        continue;
      }
    }
    auto ret = lua_resume(ctx->pulse_thread, nullptr, 0, &nargs);
    switch (ret) {
    case LUA_OK:
      ctx->pulse_yielding = false;
      break;
    case LUA_YIELD:
      ctx->pulse_yielding = true;
      break;
    case LUA_ERRRUN:
    case LUA_ERRMEM:
    case LUA_ERRSYNTAX:
    case LUA_ERRFILE:
      LOG("Received a lua error, stopping module %s.", ctx->name.c_str());
      // TODO: full backtrace/stack dump
      LOG("lua err string: %s", lua_tostring(ctx->pulse_thread, -1));
      luna_ctxs_.erase(luna_ctxs_.begin() + i);
      --i;
      break;
    default:
      LOG("2:UNKNOWN ERROR IN PULSE.");
      luna_ctxs_.erase(luna_ctxs_.begin() + i);
      --i;
      break;
    }
    if (nargs != 0) {
      // TODO
      LOG("FIXME NARGS");
    }
  }
  in_pulse_ = false;
}

void Luna::OnWriteChatColor(const char* line, std::uint32_t color, std::uint32_t filter) {}

void Luna::OnIncomingChat(const char* line, std::uint32_t color) { todo_events_.emplace_back(std::string{line}); }

void Luna::OnBeginZone() {}

void Luna::OnEndZone() {}
