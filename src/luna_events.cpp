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

void Luna::OnZoned() {
  for (auto&& ctx : luna_ctxs_) {
    ctx->zoned();
  }
}

void Luna::OnCleanUI() {}

void Luna::OnReloadUI() {
  for (auto&& ctx : luna_ctxs_) {
    ctx->reload_ui();
  }
}

void Luna::OnDrawHUD() {
  for (auto&& ctx : luna_ctxs_) {
    ctx->draw_hud();
  }
}

void Luna::SetGameState(GameState game_state) {
  for (auto&& ctx : luna_ctxs_) {
    ctx->set_game_state(game_state);
  }
}

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

    LunaContext* ctx_ptr = nullptr;
    for (const std::unique_ptr<LunaContext>& ctx : luna_ctxs_) {
      if (ctx->has_command_binding(cmd)) {
        ctx_ptr = ctx.get();
        break;
      }
    }

    if (ctx_ptr == nullptr) {
      LOG("No luna command found for '%s'", cmd_str.c_str());
      continue;
    }
    ctx_ptr->do_command_bind(std::move(args));
  }
  todo_bind_commands_.clear();
}

void Luna::do_events() {
  std::smatch cm;
  // index-based loop because it may be possible that more events are added
  // during.
  for (auto k = 0u; k < todo_events_.size(); ++k) {
    const std::string& event_str = todo_events_[k];
    for (auto&& ctx : luna_ctxs_) {
      ctx->do_event(event_str);
    }
  }
  todo_events_.clear();
}

void Luna::do_luna_commands() {
  for (auto i = 0u; i < todo_luna_cmds_.size(); ++i) {
    std::string_view sv = todo_luna_cmds_[i];
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

  cleanup_exiting_contexts();
  for (const std::unique_ptr<LunaContext>& ctx : luna_ctxs_) {
    ctx->pulse();
  }
  in_pulse_ = false;
}

void Luna::OnWriteChatColor(const char* line, std::uint32_t color, std::uint32_t filter) {}

void Luna::OnIncomingChat(const char* line, std::uint32_t color) {
  for (auto&& ctx : luna_ctxs_) {
    if (ctx->matches_event(line)) {
      todo_events_.emplace_back(std::string{line});
      return;
    }
  }
}

void Luna::OnBeginZone() {}
void Luna::OnEndZone() {}
