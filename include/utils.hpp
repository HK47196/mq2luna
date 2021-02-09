/*
 * utils.hpp Copyright © 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#ifndef UTILS_HPP80518
#define UTILS_HPP80518

#include "lua.hpp"
#include "luna_defs.hpp"
#include <string>
#include <string_view>
#include <vector>

class LunaContext;

namespace zx {
std::vector<std::string_view> strsplit(std::string_view str, std::string_view delims = " ");
LunaContext* get_context(lua_State* ls);
} // namespace zx

#endif /* !UTILS_HPP80518 */
