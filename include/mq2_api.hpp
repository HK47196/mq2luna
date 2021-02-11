/*
 * mq2_api.hpp Copyright © 2021 rsw0x
 *
 * Distributed under terms of the GPLv3 license.
 */

#ifndef MQ2_API_HPP51844
#define MQ2_API_HPP51844

#include "mq2_defines.hpp"
#include <windows.h>

#define PLUGIN_API extern "C" __declspec(dllexport)
#define EQLIB_API extern "C" __declspec(dllexport)

class MQ2Type;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
struct ARGBCOLOR {
  union {
    struct {
      BYTE B;
      BYTE G;
      BYTE R;
      BYTE A;
    };
    DWORD ARGB;
  };
};

struct MQ2VarPtr {
  // struct {
  //   PVOID Ptr;
  //   LONG HighPart;
  // };
  // struct {
  //   FLOAT Float;
  //   LONG HighPart;
  // };
  // struct {
  //   DWORD DWord;
  //   LONG HighPart;
  // };
  // struct {
  //   ARGBCOLOR Argb;
  //   LONG HighPart;
  // };
  // struct {
  //   int   Int;
  //   LONG HighPart;
  // };
  // struct {
  //   UCHAR Array[4];
  //   LONG HighPart;
  // };

  union {
    struct {
      union {
        PVOID Ptr;
        FLOAT Float;
        DWORD DWord;
        ARGBCOLOR Argb;
        int Int;
        UCHAR Array[4];
      };
      LONG HighPart;
    };
    struct {
      UCHAR FullArray[8];
    };
    DOUBLE Double;
    __int64 Int64;
    unsigned __int64 UInt64;
  };
};

struct MQ2TypeVar {
  class MQ2Type* Type;
  union {
    MQ2VarPtr VarPtr;
    struct {
      union {
        PVOID Ptr;
        FLOAT Float;
        DWORD DWord;
        ARGBCOLOR Argb;
        int Int;
        UCHAR Array[4];
      };
      LONG HighPart;
    };
    // struct {
    //   PVOID Ptr;
    //   LONG HighPart;
    // };
    // struct {
    //   FLOAT Float;
    //   LONG HighPart;
    // };
    // struct {
    //   DWORD DWord;
    //   LONG HighPart;
    // };
    // struct {
    //   ARGBCOLOR Argb;
    //   LONG HighPart;
    // };
    // struct {
    //   int   Int;
    //   LONG HighPart;
    // };
    // struct {
    //   UCHAR Array[4];
    //   LONG HighPart;
    // };
    struct {
      UCHAR FullArray[8];
    };
    DOUBLE Double;
    __int64 Int64;
    unsigned __int64 UInt64;
  };
};
#pragma GCC diagnostic pop

struct SPAWNINFO;
using PSPAWNINFO = SPAWNINFO*;

using fEQCommand = VOID(__cdecl*)(PSPAWNINFO, PCHAR Buffer);

#define MQ2_TYPES                                                                                                      \
  X(pArrayType)                                                                                                        \
  X(pBoolType)                                                                                                         \
  X(pByteType)                                                                                                         \
  X(pFloatType)                                                                                                        \
  X(pDoubleType)                                                                                                       \
  X(pIntType)                                                                                                          \
  X(pInt64Type)                                                                                                        \
  X(pMacroType)                                                                                                        \
  X(pMathType)                                                                                                         \
  X(pPluginType)                                                                                                       \
  X(pStringType)                                                                                                       \
  X(pTimeType)                                                                                                         \
  X(pTypeType)                                                                                                         \
  X(pEverQuestType)                                                                                                    \
  X(pSpawnType)                                                                                                        \
  X(pSpellType)

class MQ2 {
public:
  MQ2();

  BOOL ParseMQ2DataPortion(const char* data_var, MQ2TypeVar& result);
  VOID WriteChatColor(const char* Line, DWORD Color = USERCOLOR_DEFAULT, DWORD Filter = 0);

  void DoCommand(PSPAWNINFO pChar, const char* cmd);
  void DoCommand(const char* cmd);
  inline PSPAWNINFO pLocalPlayer() { return ppLocalPlayer ? *ppLocalPlayer : nullptr; }

  DWORD GetGameState(VOID);
  VOID AddCommand(const char* cmd, fEQCommand fn, BOOL EQ = 0, BOOL Parse = 1, BOOL InGame = 0);
  VOID RemoveCommand(const char* cmd);

#define X(type) MQ2Type* type;
  MQ2_TYPES
#undef X

  PSPAWNINFO* ppLocalPlayer;
  const char* mq2_dir;

private:
  BOOL (*ParseMQ2DataPortionFP)(PCHAR szOriginal, MQ2TypeVar& result) = nullptr;
  VOID (*WriteChatColorFP)(PCHAR Line, DWORD Color, DWORD Filter) = nullptr;
  VOID (*HideDoCommandFP)(PSPAWNINFO pChar, PCHAR szLine, BOOL delayed) = nullptr;
  DWORD (*GetGameStateFP)() = nullptr;
  VOID (*AddCommandFP)(const char* cmd, fEQCommand fn, BOOL EQ, BOOL Parse, BOOL InGame) = nullptr;
  VOID (*RemoveCommandFP)(const char* cmd) = nullptr;
};

extern MQ2* mq2;

#endif /* !MQ2_API_HPP51844 */
