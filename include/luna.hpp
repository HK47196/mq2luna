/*
 * luna.hpp Copyright © 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#ifndef LUNA_HPP23917
#define LUNA_HPP23917

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "luna_context.hpp"
#include "luna_defs.hpp"

namespace fs = std::filesystem;

enum class GameState : std::uint32_t {
  GAMESTATE_CHARSELECT = 1,
  GAMESTATE_CHARCREATE = 2,
  GAMESTATE_SOMETHING = 4,
  GAMESTATE_INGAME = 5,
  GAMESTATE_PRECHARSELECT = -1u,
  GAMESTATE_POSTFRONTLOAD = 500,
  GAMESTATE_LOGGINGIN = 253,
  GAMESTATE_UNLOADING = 255,
};

namespace zx {
extern char scratch_buf[4096];
static_assert(sizeof(scratch_buf) == 4096);
} // namespace zx

#ifndef LOG
#define LOG(fmt_string, ...)                                                                                           \
  do {                                                                                                                 \
    std::snprintf(::zx::scratch_buf, sizeof(::zx::scratch_buf),                                                        \
                  "\ag[Luna]\ax " fmt_string __VA_OPT__(, ) __VA_ARGS__);                                              \
    mq2->WriteChatColor(::zx::scratch_buf);                                                                            \
  } while (false);
#endif

#ifndef DLOG
#define DLOG(fmt_string, ...)                                                                                          \
  do {                                                                                                                 \
    if (debug_) {                                                                                                      \
      std::snprintf(::zx::scratch_buf, sizeof(::zx::scratch_buf),                                                      \
                    "\ay[Luna:Debug]\ax " fmt_string __VA_OPT__(, ) __VA_ARGS__);                                      \
      mq2->WriteChatColor(::zx::scratch_buf);                                                                          \
    }                                                                                                                  \
  } while (false);
#endif

class Luna {
public:
  Luna();
  ~Luna();

  void OnZoned();
  void OnCleanUI();
  void OnReloadUI();
  void OnDrawHUD();
  void SetGameState(GameState game_state);
  void OnPulse();
  void OnWriteChatColor(const char* line, std::uint32_t color, std::uint32_t filter);
  void OnIncomingChat(const char* line, std::uint32_t color);
  void OnBeginZone();
  void OnEndZone();

  void Cmd(const char* cmd);
  void BoundCommand(const char* cmd);
  inline bool in_pulse() const { return in_pulse_; }
  int add_bind(lua_State* ls);
  void add_event(lua_State* ls);

  void remove_bind(const std::string& cmd);

private:
  void print_info();
  void print_help();
  void list_available_modules();

  void run_module(std::string_view sv);
  void stop_module(std::string_view sv);
  void pause_module(std::string_view sv);

  int find_index_of(std::string_view ctx_name);

  void load_config();
  void save_config();

  void do_binds();
  void do_events();
  void do_luna_commands();

  bool in_pulse_ = false;
  bool debug_ = false;

  std::vector<std::unique_ptr<LunaContext>> luna_ctxs_;
  fs::path modules_dir;
  std::vector<std::string> todo_bind_commands_;
  std::vector<std::string> todo_events_;
  std::vector<std::string> todo_luna_cmds_;
  std::map<std::string, std::pair<LunaContext*, int>, std::less<>> bound_command_map_;

  std::chrono::time_point<std::chrono::high_resolution_clock> last_tick_;
};

extern Luna* luna;

#endif /* !LUNA_HPP23917 */
