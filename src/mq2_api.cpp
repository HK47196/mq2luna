/*
 * mq2_api.cpp
 * Copyright (C) 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#include "mq2_api.hpp"
#include <cstring>
#include <libloaderapi.h>

__declspec(dllexport) float MQ2Version = 0.1f;
#define MAX_STRING 2048

MQ2* mq2;

namespace {
char scratch_buf[4096] = {'\0'};
}

MQ2::MQ2() {
  auto mq2_module = GetModuleHandle("MQ2Main.dll");
  if (mq2_module == nullptr) {
    return;
  }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
  ParseMQ2DataPortionFP = (decltype(ParseMQ2DataPortionFP))GetProcAddress(mq2_module, "ParseMQ2DataPortion");
  WriteChatColorFP = (decltype(WriteChatColorFP))GetProcAddress(mq2_module, "WriteChatColor");
  HideDoCommandFP = (decltype(HideDoCommandFP))GetProcAddress(mq2_module, "HideDoCommand");
  GetGameStateFP = (decltype(GetGameStateFP))GetProcAddress(mq2_module, "GetGameState");
  AddCommandFP = (decltype(AddCommandFP))GetProcAddress(mq2_module, "AddCommand");
  RemoveCommandFP = (decltype(RemoveCommandFP))GetProcAddress(mq2_module, "RemoveCommand");
#pragma GCC diagnostic pop
#define X(var) var = *(MQ2Type**)GetProcAddress(mq2_module, #var);
  MQ2_TYPES
#undef X
  ppLocalPlayer = *(PSPAWNINFO**)GetProcAddress(mq2_module, "ppLocalPlayer");
  char path[MAX_PATH];
  if (GetModuleFileName(mq2_module, path, sizeof(path)) == 0) {
    mq2_dir = nullptr;
  } else {
    mq2_dir = strdup(path);
  }
}

BOOL MQ2::ParseMQ2DataPortion(const char* data_var, MQ2TypeVar& result) {
  if (ParseMQ2DataPortionFP == nullptr) {
    return false;
  }
  std::strcpy(scratch_buf, data_var);
  return ParseMQ2DataPortionFP(scratch_buf, result);
}

VOID MQ2::WriteChatColor(const char* Line, DWORD Color, DWORD Filter) {
  if (WriteChatColorFP == nullptr) {
    return;
  }
  std::strcpy(scratch_buf, Line);
  WriteChatColorFP(scratch_buf, Color, Filter);
}

void MQ2::DoCommand(PSPAWNINFO pChar, const char* szLine) {
  if (pChar == nullptr || HideDoCommandFP == nullptr) {
    return;
  }
  std::strcpy(scratch_buf, szLine);
  HideDoCommandFP(pChar, scratch_buf, true);
}

void MQ2::DoCommand(const char* szLine) { DoCommand(pLocalPlayer(), szLine); }

DWORD MQ2::GetGameState(VOID) {
  if (GetGameStateFP == nullptr) {
    return 0;
  }
  return GetGameStateFP();
}

VOID MQ2::AddCommand(const char* cmd, fEQCommand fn, BOOL EQ, BOOL Parse, BOOL InGame) {
  if (AddCommandFP == nullptr) {
    WriteChatColor("\arLuna failed to load function AddCommand!");
    return;
  }
  std::strcpy(scratch_buf, cmd);
  AddCommandFP(scratch_buf, fn, EQ, Parse, InGame);
}

VOID MQ2::RemoveCommand(const char* cmd) {
  if (RemoveCommandFP == nullptr) {
    WriteChatColor("\arLuna failed to load function RemoveCommand!");
    return;
  }
  std::strcpy(scratch_buf, cmd);
  RemoveCommandFP(scratch_buf);
}
