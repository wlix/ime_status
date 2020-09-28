#pragma once

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <string>

#define PROP_OLDPROC    "IMEStatusProcedure"
#define FILEMAP_SHARED  L"IME_STATUS"

typedef struct _SHARED_DATA {
    // 除外するウィンドウのパスの配列
    UINT on;
    // 除外するウィンドウのパスの個数
    UINT off;
    // iniのパス
    wchar_t x86_inipath[MAX_PATH];
    wchar_t x64_inipath[MAX_PATH];
} SHARED_DATA;

