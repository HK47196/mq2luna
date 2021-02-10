/*
 * luna_defs.hpp Copyright © 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#ifndef LUNA_DEFS_HPP74145
#define LUNA_DEFS_HPP74145

#include <cstdint>

static constexpr const char* LUNA_MODULE_KEY = "luna_module_name";
static constexpr const char* LUNA_CTX_PTR_KEY = "lua_ctx_ptr";

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


#endif /* !LUNA_DEFS_HPP74145 */
