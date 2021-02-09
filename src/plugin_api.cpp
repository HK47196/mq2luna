/*
 * plugin_api.cpp
 * Copyright (C) 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#include "luna.hpp"
#include "mq2_api.hpp"
#include <windows.h>

#define PLUGIN_API extern "C" __declspec(dllexport)

#include <cstring>

#include <stdio.h>

void LunaCmd(PSPAWNINFO, PCHAR cmd) {
  if (!luna || !mq2) {
    return;
  }
  luna->Cmd(cmd);
}

void LunaDo(PSPAWNINFO, PCHAR cmd) {
  if (!luna || !mq2) {
    return;
  }
  luna->BoundCommand(cmd);
}

char buf[4096];
PLUGIN_API VOID InitializePlugin(VOID) {
  mq2 = new MQ2();
  luna = new Luna();

  mq2->AddCommand("/luna", LunaCmd, false, true, false);
  mq2->AddCommand("/ldo", LunaDo, false, true, false);
}

PLUGIN_API VOID ShutdownPlugin(VOID) {
  mq2->RemoveCommand("/ldo");
  mq2->RemoveCommand("/luna");
  delete luna;
  luna = nullptr;
  delete mq2;
  mq2 = nullptr;
}

PLUGIN_API VOID OnZoned(VOID) {
  if (luna) {
    luna->OnZoned();
  }
}

PLUGIN_API VOID OnCleanUI(VOID) {
  if (luna) {
    luna->OnCleanUI();
  }
}

PLUGIN_API VOID OnReloadUI(VOID) {
  if (luna) {
    luna->OnReloadUI();
  }
}

PLUGIN_API VOID OnDrawHUD(VOID) {
  if (luna) {
    luna->OnDrawHUD();
  }
}

PLUGIN_API VOID SetGameState(DWORD game_state) {
  if (luna) {
    luna->SetGameState(GameState{game_state});
  }
}

PLUGIN_API VOID OnPulse(VOID) {
  if (luna) {
    luna->OnPulse();
  }
}

PLUGIN_API DWORD OnWriteChatColor(PCHAR Line, DWORD Color, DWORD Filter) {
  if (luna) {
    luna->OnWriteChatColor(Line, Color, Filter);
  }
  return 0;
}

PLUGIN_API DWORD OnIncomingChat(PCHAR Line, DWORD Color) {
  if (luna) {
    luna->OnIncomingChat(Line, Color);
  }
  return 0;
}

// PLUGIN_API VOID OnAddSpawn(PSPAWNINFO pNewSpawn) {
// }
//
// PLUGIN_API VOID OnRemoveSpawn(PSPAWNINFO pSpawn) {
// }

// PLUGIN_API VOID OnAddGroundItem(PGROUNDITEM pNewGroundItem)
// {
// }
//
// PLUGIN_API VOID OnRemoveGroundItem(PGROUNDITEM pGroundItem)
// {
// }

PLUGIN_API VOID OnBeginZone(VOID) {
  if (luna) {
    luna->OnBeginZone();
  }
}

PLUGIN_API VOID OnEndZone(VOID) {
  if (luna) {
    luna->OnEndZone();
  }
}
