/*
 * utils.cpp
 * Copyright (C) 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#include "utils.hpp"

namespace zx {
std::vector<std::string_view> strsplit(std::string_view str, std::string_view delims) {
  std::vector<std::string_view> output;

  for (auto first = str.data(), second = str.data(), last = first + str.size(); second != last && first != last;
       first = second + 1) {
    second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

    if (first != second)
      output.emplace_back(first, second - first);
  }

  return output;
}

LunaContext* get_context(lua_State* ls) {
  lua_getfield(ls, LUA_REGISTRYINDEX, LUNA_CTX_PTR_KEY);
  auto p = lua_touserdata(ls, -1);
  lua_pop(ls, 1);
  if (p == nullptr) {
    luaL_error(ls, "No ptr registered for this context??");
    return nullptr;
  }
  return static_cast<LunaContext*>(p);
}
} // namespace zx
