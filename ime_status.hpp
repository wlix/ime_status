#pragma once

#include <SDKDDKVer.h>

#include <windows.h>
#include <string>

#define PROP_OLDPROC    TEXT("IMEStatusProcedure")

typedef struct _CONFIG_DATA {
    // 除外するウィンドウのパスの配列
    UINT on;
    // 除外するウィンドウのパスの個数
    UINT off;
} CONFIG_DATA;
