﻿//---------------------------------------------------------------------------//
//
// Main.cpp
//
//---------------------------------------------------------------------------//

#include "Plugin.hpp"
#include "MessageDef.hpp"
#include "Utility.hpp"

#include "config.hpp"

//---------------------------------------------------------------------------//
//
// グローバル変数
//
//---------------------------------------------------------------------------//

#pragma data_seg(".SHARED_DATA")
HHOOK  g_hHook  { nullptr };
#pragma data_seg()

HINSTANCE    g_hInst  { nullptr };
HANDLE       g_hShared{ nullptr };
SHARED_DATA *g_Shared { nullptr };

// プラグインの名前
#if defined(WIN64) || defined(_WIN64)
LPWSTR PLUGIN_NAME  { L"IME Status for Win10 x64" };
#else
LPSTR  PLUGIN_NAME  {  "IME Status for Win10 x86" };
#endif

// コマンドの数
DWORD COMMAND_COUNT { 0 };

// プラグインの情報
PLUGIN_INFO g_info = {
    0,                   // プラグインI/F要求バージョン
    PLUGIN_NAME,         // プラグインの名前（任意の文字が使用可能）
    nullptr,             // プラグインのファイル名（相対パス）
    ptAlwaysLoad,        // プラグインのタイプ
    0,                   // バージョン
    0,                   // バージョン
    COMMAND_COUNT,       // コマンド個数
    NULL,                // コマンド
    0,                   // ロードにかかった時間（msec）
};


// --------------------------------------------------------
//    フックプロシージャ
// --------------------------------------------------------
LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        LPCWPSTRUCT pcw = (LPCWPSTRUCT)lParam;
        if ((pcw->message == WM_IME_NOTIFY && pcw->wParam == IMN_SETOPENSTATUS)
            || pcw->message == WM_SETFOCUS) {
            HIMC hIMC = ImmGetContext(pcw->hwnd);
            if (hIMC) {
                if (ImmGetOpenStatus(hIMC)) {
                    SetCaretBlinkTime(g_Shared->on);
                }
                else {
                    SetCaretBlinkTime(g_Shared->off);
                }
                ImmReleaseContext(pcw->hwnd, hIMC);
            }
    }
    }
    return ::CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// TTBEvent_Init() の内部実装
BOOL WINAPI Init(void) {
    config::get_instance().load_config();

    g_hHook = ::SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, g_hInst, 0);
    if (!g_hHook) {
        DWORD ret = GetLastError();
        WriteLog(elError, TEXT("%s: フックに失敗しました"), g_info.Name);
        return FALSE;
    }
    // ::PostMessageA(HWND_BROADCAST, WM_NULL, 0, 0);

    return TRUE;
}

//---------------------------------------------------------------------------//

// TTBEvent_Unload() の内部実装
void WINAPI Unload(void) {
    ::UnhookWindowsHookEx(g_hHook);
    g_hHook = NULL;
    // ::PostMessageA(HWND_BROADCAST, WM_NULL, 0, 0);
    WriteLog(elInfo, TEXT("%s: successfully uninitialized"), g_info.Name);
}

//---------------------------------------------------------------------------//

// TTBEvent_Execute() の内部実装
BOOL WINAPI Execute(INT32 CmdId, HWND hWnd) {
    return TRUE;
}

//---------------------------------------------------------------------------//

// TTBEvent_WindowsHook() の内部実装
void WINAPI Hook(UINT Msg, WPARAM wParam, LPARAM lParam) {
}

//---------------------------------------------------------------------------//
//
// CRT を使わないため new/delete を自前で実装
//
//---------------------------------------------------------------------------//

#if defined(_NODEFLIB)

void* __cdecl operator new(size_t size)
{
    return ::HeapAlloc(::GetProcessHeap(), 0, size);
}

void __cdecl operator delete(void* p)
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

void __cdecl operator delete(void* p, size_t) // C++14
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

void* __cdecl operator new[](size_t size)
{
    return ::HeapAlloc(::GetProcessHeap(), 0, size);
}

void __cdecl operator delete[](void* p)
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

void __cdecl operator delete[](void* p, size_t) // C++14
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

// プログラムサイズを小さくするためにCRTを除外
#pragma comment(linker, "/nodefaultlib:libcmt.lib")
#pragma comment(linker, "/entry:DllMain")

#endif

//---------------------------------------------------------------------------//

// DLL エントリポイント
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        g_hInst = hInstance;
        // 共有メモリを作成
        g_hShared = ::CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SHARED_DATA), FILEMAP_SHARED);
        if (!g_hShared) {
            WriteLog(elError, TEXT("%s: 共有メモリの作成に失敗しました"), g_info.Name);
            return FALSE;
        } else {
            g_Shared = (SHARED_DATA *)::MapViewOfFile(g_hShared, FILE_MAP_WRITE, 0, 0, 0);
            if (!g_Shared) {
                WriteLog(elError, TEXT("%s: 共有メモリのマッピングに失敗しました"), g_info.Name);
                return FALSE;
            } else if (GetLastError() != ERROR_ALREADY_EXISTS) {
                ::SecureZeroMemory((PVOID)g_Shared, sizeof(SHARED_DATA));
            }
        }
        break;

    case DLL_PROCESS_DETACH:
        // 共有メモリをクローズ
        ::UnmapViewOfFile(g_Shared);
        ::CloseHandle(g_hShared);
        break;
    }
    return TRUE;
}
