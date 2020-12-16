#include "pch.h"
#include <imm.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "ui_automation_client.hpp"

#include <memory>
#include <MinHook.h>

#include "config.hpp"
#include "ime_status.hpp"

#include "Plugin.hpp"
#include "MessageDef.hpp"
#include "Utility.hpp"

BOOL IsImeEnabled(HWND hWnd);
BOOL GetConversionStatus(HWND hWnd, DWORD *pConversion);
void FocusChangedHandler(IUIAutomationElement *sender);

#pragma data_seg(".SHARED_DATA")
HHOOK  g_hHook = nullptr;
#pragma data_seg()

CAppModule _Module;

HINSTANCE   g_hInst              = nullptr;
CONFIG_DATA g_Config             = {};
BOOL        g_bAPIHooked         = FALSE;
BOOL        g_bTargetIsWow64     = FALSE;
UINT        g_default_blink_time = 0;

struct CARET_INFO {
    BOOL    bCreated = FALSE;
    HBITMAP hBmp = NULL;
    LONG    lockedConut = 0;
    struct {
        HBITMAP hBmp = NULL;
        int     width = 0;
        int     height = 0;
    } args;
} g_CaretInfo, g_PreCaretInfo;

typedef BOOL(WINAPI* pfCreateCaret) (HWND, HBITMAP, int, int);
typedef BOOL(WINAPI* pfShowCaret)   (HWND);
typedef BOOL(WINAPI* pfHideCaret)   (HWND);
typedef BOOL(WINAPI* pfDestroyCaret)();

pfCreateCaret  WinAPI_CreateCaret  = NULL;
pfShowCaret    WinAPI_ShowCaret    = NULL;
pfHideCaret    WinAPI_HideCaret    = NULL;
pfDestroyCaret WinAPI_DestroyCaret = NULL;

std::unique_ptr<CUIAutomationClient> g_ui_automation;

// プラグインの名前
#if defined(WIN64) || defined(_WIN64)
LPCTSTR PLUGIN_NAME{ TEXT("IME Status for Win10 x64") };
#else
LPCTSTR PLUGIN_NAME{ TEXT("IME Status for Win10 x86") };
#endif

// コマンドの数
DWORD COMMAND_COUNT{ 0 };

// プラグインの情報
PLUGIN_INFO g_info = {
    0,                   // プラグインI/F要求バージョン
    (LPTSTR)PLUGIN_NAME, // プラグインの名前（任意の文字が使用可能）
    nullptr,             // プラグインのファイル名（相対パス）
    ptAlwaysLoad,        // プラグインのタイプ
    0,                   // バージョン
    0,                   // バージョン
    COMMAND_COUNT,       // コマンド個数
    NULL,                // コマンド
    0,                   // ロードにかかった時間（msec）
};

LPCTSTR CCaretWndClassName = TEXT("CaretWndForIMEStatus_");
typedef CWinTraits<WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST>	CCaretWinTraits;
class CCaretWnd : public CDoubleBufferWindowImpl<CCaretWnd, CWindow, CCaretWinTraits> {
public:
    DECLARE_WND_CLASS(CCaretWndClassName)

    const int TimerId = 1;
    const int TimerInterval = 100;

    void SetSender(IUIAutomationElement* sender) {
        m_sp_sender = sender;
        if (sender) {
            // WriteLog(elDebug, TEXT("SetTimer"));
            SetTimer(TimerId, TimerInterval);
            SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
        }
        else {
            // WriteLog(elDebug, TEXT("KillTimer"));
            KillTimer(TimerId);
            ShowWindow(SW_HIDE);
        }
    }

    void ChangeIMEState(BOOL cond) {
        // WriteLog(elDebug, TEXT("ChangeIMEState cond: %d, m_ime_on: %d"), cond, m_ime_on);
        if (m_ime_on != cond) {
            m_ime_on = cond;
            if (m_ime_on) {
                // WriteLog(elDebug, TEXT("GetBlinkTime: %d"), ::GetCaretBlinkTime());
                if (::GetCaretBlinkTime() != g_Config.on) {
                    // WriteLog(elDebug, TEXT("SetBlinkTime: %d"), g_Config.on);
                    ::SetCaretBlinkTime(g_Config.on);
                }
            }
            else {
                // WriteLog(elDebug, TEXT("GetBlinkTime: %d"), ::GetCaretBlinkTime());
                if (::GetCaretBlinkTime() != g_Config.off) {
                    // WriteLog(elDebug, TEXT("SetBlinkTime: %d"), g_Config.off);
                    ::SetCaretBlinkTime(g_Config.off);
                }
            }
            Invalidate(FALSE);
        }
    }

    BEGIN_MSG_MAP_EX(CCaretWnd)
        MSG_WM_CREATE(OnCreate)
        MESSAGE_HANDLER_EX(WM_SHOWCARETIFNEED, OnShowCaretIfNeed)
        MSG_WM_TIMER(OnTimer)
        CHAIN_MSG_MAP(__super)
    END_MSG_MAP()

    int OnCreate(LPCREATESTRUCT) {
        return 0;
    }

    LRESULT OnShowCaretIfNeed(UINT, WPARAM wParam, LPARAM lParam) {
        if (lParam != 0) {
            SetSender(nullptr);
            return 0;
        }
        // WriteLog(elDebug, TEXT("From OnShowCaretIfNeed"));
        ChangeIMEState(wParam != 0);
        return 0;
    }

    void DoPaint(CDCHandle dc) {
        RECT rc;
        GetClientRect(&rc);
        if (m_ime_on) {
            dc.FillSolidRect(&rc, 0x00FFFFFF);
        }
        else {
            dc.FillSolidRect(&rc, 0x00FFFFFF);
        }
    }

    void OnTimer(UINT_PTR nIDEvent) {
        if (nIDEvent != TimerId) { return; }
#if defined(WIN64) || defined(_WIN64)
        if (g_bTargetIsWow64)    { return; }
#else
        if (!g_bTargetIsWow64)   { return; }
#endif

        HWND hForeWnd = ::GetForegroundWindow();
        CRect rcForeground;
        ::GetWindowRect(hForeWnd, &rcForeground);
        DWORD threadId = ::GetWindowThreadProcessId(hForeWnd, nullptr);
        GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
        ::GetGUIThreadInfo(threadId, &gti);

        HWND HImeWnd = ::ImmGetDefaultIMEWnd(gti.hwndCaret);
        LRESULT ret = ::SendMessage(HImeWnd, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0);
        // WriteLog(elDebug, TEXT("From Timer"));
        ChangeIMEState(ret != 0);
    }

private:
    CComPtr<IUIAutomationElement> m_sp_sender;
    BOOL m_ime_on = FALSE;
} g_CaretWnd;

class WinAPI_HookManager {
public:
    WinAPI_HookManager() : m_hHookEngineDll(NULL) {
        if (MH_Initialize() == MH_OK) {
            g_bAPIHooked = WinAPIHook();
        }
    }
    ~WinAPI_HookManager() {
        if (g_bAPIHooked) {
            WinAPIUnHook();
            MH_Uninitialize();
        }
    }

    BOOL WinAPIHook();
    void WinAPIUnHook();

private:
	HMODULE m_hHookEngineDll;
};

// --------------------------------------------------------
//    フックプロシージャ
// --------------------------------------------------------
LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        LPCWPSTRUCT pcw = (LPCWPSTRUCT)lParam;
        if (pcw->message == WM_IME_NOTIFY && pcw->wParam == IMN_SETOPENSTATUS) {
            // WriteLog(elDebug, TEXT("Hook SetOpenStatus"));
            int Count = g_CaretInfo.lockedConut;

            HWND hForeWnd = ::GetForegroundWindow();
            DWORD threadId = ::GetWindowThreadProcessId(hForeWnd, nullptr);
            GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
            ::GetGUIThreadInfo(threadId, &gti);

            // WriteLog(elDebug, TEXT("pcw->hwnd: %08x, gti.hwndFocus: 08x"), pcw->hwnd, gti.hwndFocus);
            int Width = 0;
            int Height = 0;
            if (gti.hwndCaret) {
                Width = gti.rcCaret.right - gti.rcCaret.left;
                Height = gti.rcCaret.bottom - gti.rcCaret.top;
            }
            else {
                Width = Height = 0;
            }

            POINT pt;
            ::GetCaretPos(&pt);
            ::DestroyCaret();
            ::CreateCaret(gti.hwndFocus ? gti.hwndFocus : pcw->hwnd, NULL, Width, Height);
            // WriteLog(elDebug, TEXT("CreateCaret: wnd30C4: %08x, Width: %d, Height: %d"), gti.hwndFocus ? gti.hwndFocus : NULL, Width, Height);
            ::SetCaretPos(pt.x, pt.y);

            HWND HCaretWnd = ::FindWindow(CCaretWndClassName, nullptr);
            // WriteLog(elDebug, TEXT("FindWindow"));
            if (HCaretWnd) {
                // WriteLog(elDebug, TEXT("Detected CCaretWndClass: %08x"), HCaretWnd);
                // WriteLog(elDebug, TEXT("PostMessage: hwnd %08x, IsImeEnabled %d"), gti.hwndFocus ? gti.hwndFocus : pcw->hwnd, IsImeEnabled(gti.hwndFocus ? gti.hwndFocus : pcw->hwnd));
                ::PostMessage(HCaretWnd, WM_SHOWCARETIFNEED, IsImeEnabled(gti.hwndFocus ? gti.hwndFocus : pcw->hwnd), 0);
            }

            // WriteLog(elDebug, TEXT("Count: %d"), Count);
            if (Count > 0) {
                for (int i = 0; i < Count; ++i) {
                    // WriteLog(elDebug, TEXT("ShowCaret: %08x, Count: %d"), gti.hwndFocus, Count);
                    ::ShowCaret(gti.hwndFocus);
                }
            }
            else if (Count < 0) {
                for (LONG i = 0; i > Count; --i) {
                    // WriteLog(elDebug, TEXT("HideCaret: %08x, Count: %d"), gti.hwndFocus, Count);
                    ::HideCaret(gti.hwndFocus);
                }
            }
        }
        else if (pcw->message == WM_IME_SETCONTEXT && pcw->wParam == FALSE) {
            // WriteLog(elDebug, TEXT("Hook SetContext"));
            HIMC hContext = ::ImmGetContext(pcw->hwnd);

            BOOL hideCaret = (hContext == NULL);
            HWND HCaretWnd = ::FindWindow(CCaretWndClassName, nullptr);
            // WriteLog(elDebug, TEXT("Findow Window: %s"), CCaretWndClassName);
            if (HCaretWnd && hideCaret) {
                // WriteLog(elDebug, TEXT("PostMessage: WM_SHOWCARETIFNEED"));
                ::PostMessage(HCaretWnd, WM_SHOWCARETIFNEED, 0, hideCaret);
            }
        } else if (pcw->message == WM_SETFOCUS) {
            // WriteLog(elDebug, TEXT("Hook WM_SETFOCUS"));
            static WinAPI_HookManager mng;
        }
    }
    return ::CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

void FocusChangedHandler(IUIAutomationElement* sender) {
    // WriteLog(elDebug, TEXT("Begin FocusChangedHandler sender: %08x"), sender);

    HWND hForeWnd = ::GetForegroundWindow();
    DWORD processId = 0;
    DWORD threadId = ::GetWindowThreadProcessId(hForeWnd, &processId);
    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    ::GetGUIThreadInfo(threadId, &gti);

    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    IsWow64Process(hProcess, &g_bTargetIsWow64);

    if (!gti.hwndFocus) {
        // WriteLog(elDebug, TEXT("hwnd:%08x is 0"), gti.hwndFocus);
        g_CaretWnd.SetSender(nullptr);
        return;
    }
#if defined(WIN64) || defined(_WIN64)
    if (g_bTargetIsWow64) {
#else
    if (!g_bTargetIsWow64) {
#endif
        // WriteLog(elDebug, TEXT("hwnd:%08x is not target architecture application"), hForeWnd);
        // WriteLog(elDebug, TEXT("g_default_blink_time: %d"), g_default_blink_time);
        return;
    }

    CONTROLTYPEID controlTypeId = 0;
    sender->get_CurrentControlType(&controlTypeId);
    // WriteLog(elDebug, TEXT("TypeId: %d"), controlTypeId);
    if (controlTypeId != UIA_EditControlTypeId
        && controlTypeId != UIA_ComboBoxControlTypeId
        && controlTypeId != UIA_CustomControlTypeId
        && controlTypeId != UIA_PaneControlTypeId) {
        g_CaretWnd.SetSender(nullptr);
        return;
    }
    if (controlTypeId == UIA_CustomControlTypeId) {
        return;
    }

    HWND hEditWnd = NULL;
    sender->get_CurrentNativeWindowHandle((UIA_HWND *)&hEditWnd);

    if (controlTypeId == UIA_ComboBoxControlTypeId) {
        if (hEditWnd) {
            DWORD windowStyle = ::GetWindowLong(hEditWnd, GWL_STYLE);
            if (windowStyle & CBS_DROPDOWNLIST) {
                g_CaretWnd.SetSender(nullptr);
                return;
            }
        }
        else {
            CComPtr<IUIAutomationValuePattern> spValuePattern;
            sender->GetCurrentPatternAs(UIA_ValuePatternId, IID_IUIAutomationValuePattern, (void**)&spValuePattern);
            if (spValuePattern) {
                BOOL bIsReadOnly = FALSE;
                spValuePattern->get_CurrentIsReadOnly(&bIsReadOnly);
                if (bIsReadOnly) {
                    g_CaretWnd.SetSender(nullptr);
                    return;
                }
            }
            else {
                CComVariant vControlType = UIA_EditControlTypeId;
                CComPtr<IUIAutomationCondition> spCndEdit;
                g_ui_automation->GetUIAutomation()->CreatePropertyCondition(UIA_ControlTypePropertyId, vControlType, &spCndEdit);
                if (spCndEdit) {
                    CComPtr<IUIAutomationElement> spChildElm;
                    sender->FindFirst(
                        TreeScope_Children, spCndEdit, &spChildElm);
                    if (spChildElm == nullptr) {
                        g_CaretWnd.SetSender(nullptr);
                        return;
                    }
                }
            }
        }
    }
    else if (controlTypeId == UIA_EditControlTypeId) {
        /*if (hEditWnd) {
            TCHAR className[128] = TEXT("");
            ::GetClassName(hEditWnd, className, 128);
            if (::_tcscmp(className, TEXT("Edit")) == 0) {
                g_CaretWnd.SetSender(nullptr);
                return;
            }
        }*/

        CComPtr<IUIAutomationValuePattern> spValuePattern;
        sender->GetCurrentPatternAs(UIA_ValuePatternId, IID_IUIAutomationValuePattern, (void**)&spValuePattern);
        if (spValuePattern) {
            BOOL bIsReadOnly = FALSE;
            spValuePattern->get_CurrentIsReadOnly(&bIsReadOnly);
            if (bIsReadOnly) {
                g_CaretWnd.SetSender(nullptr);
                return;
            }
        }
    }

    if (gti.flags != GUI_CARETBLINKING && hEditWnd) {
        g_CaretWnd.SetSender(nullptr);
        return;
    }

    HWND hIMEWnd = ::ImmGetDefaultIMEWnd(gti.hwndFocus);
    BOOL ret = ::SendMessage(hIMEWnd, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0);
    // WriteLog(elDebug, TEXT("From Focus Handler"));
    g_CaretWnd.ChangeIMEState(ret != 0);

    g_CaretWnd.SetSender(sender);
}

// TTBEvent_Init() の内部実装
BOOL WINAPI Init(void) {
    config::get_instance().load_config();

    DWORD dwResult = 0;
    HKEY  hKey = NULL;
    dwResult = ::RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_QUERY_VALUE, &hKey);
    if (dwResult != ERROR_SUCCESS) {
        WriteLog(elError, TEXT("%s: レジストリを読み込めませんでした"), g_info.Name);
    }
    else {
        // WriteLog(elDebug, TEXT("%08x"), hKey);
        WCHAR szReadBuf[128] = L"";
        DWORD dwReadSize = sizeof(szReadBuf);
        dwResult = ::RegQueryValueExW(hKey, L"CursorBlinkRate", 0, NULL, (LPBYTE)szReadBuf, &dwReadSize);
        if (dwResult != ERROR_SUCCESS) {
            WriteLog(elError, TEXT("%s: レジストリを読み込めませんでした"), g_info.Name);
        }
        else {
            g_default_blink_time = ::_wtoi(szReadBuf);
            // WriteLog(elDebug, TEXT("%s: default blink time: %d"), g_info.Name, g_default_blink_time);
        }
    }
    if (hKey) { ::RegCloseKey(hKey); }
    if (g_default_blink_time == 0) { g_default_blink_time = 530; }

    g_hHook = ::SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, g_hInst, 0);
    if (!g_hHook) {
        WriteLog(elError, TEXT("%s: フックに失敗しました"), g_info.Name);
        return FALSE;
    }

    SYSTEM_INFO sysInfo = {};
    ::GetNativeSystemInfo(&sysInfo);

    BOOL bIsWow64 = FALSE;
    ::IsWow64Process(GetCurrentProcess(), &bIsWow64);

    if ((sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL || bIsWow64)
        || (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 && !bIsWow64)) {
        g_CaretWnd.Create(NULL);
        g_CaretWnd.MoveWindow(200, 200, 10, 50);

        g_ui_automation.reset(new CUIAutomationClient);
        g_ui_automation->AddFocusChangedEventHandler(FocusChangedHandler);
    }

    return TRUE;
}

//---------------------------------------------------------------------------//

// TTBEvent_Unload() の内部実装
void WINAPI Unload(void) {
    if (g_hHook) {
        ::UnhookWindowsHookEx(g_hHook);
    }
    g_hHook = nullptr;

    if (g_CaretWnd.IsWindow()) {
        g_ui_automation.reset();
        g_CaretWnd.DestroyWindow();
    }

    ::PostMessageA(HWND_BROADCAST, WM_NULL, 0, 0);
    WriteLog(elInfo, TEXT("%s: successfully uninitialized"), g_info.Name);
}

//---------------------------------------------------------------------------//

// TTBEvent_Execute() の内部実装
BOOL WINAPI Execute(INT32, HWND) {
    return TRUE;
}

//---------------------------------------------------------------------------//

// TTBEvent_WindowsHook() の内部実装
void WINAPI Hook(UINT, WPARAM, LPARAM) {
}

void InitCaretInfo(CARET_INFO *info) {
	if (info->hBmp) {
		::DeleteObject(info->hBmp);
        info->hBmp = NULL;
	}
    info->lockedConut = 0;
    info->bCreated = FALSE;
    if (info->args.hBmp) {
        ::DeleteObject(info->args.hBmp);
        info->hBmp = NULL;
    }
    info->args.width = 0;
    info->args.height = 0;
}

BOOL IsImeEnabled(HWND hWnd) {
    DWORD Conversion = 0;
    GetConversionStatus(hWnd, &Conversion);
    if (Conversion == IME_CMODE_ALPHANUMERIC || Conversion == IME_CMODE_NOCONVERSION ) {
        return FALSE;
    }
    return TRUE;
}

BOOL GetConversionStatus(HWND hWnd, DWORD* pConversion) {
    if (pConversion == nullptr) { return FALSE; }
    HIMC hImc = ::ImmGetContext(hWnd);
    if (hImc) {
        if (::ImmGetOpenStatus(hImc)) {
            DWORD dwConversion = 0;
            DWORD dwSentence = 0;
            if (::ImmGetConversionStatus(hImc, &dwConversion, &dwSentence)) {
                *pConversion = dwConversion;
            }
        }
        ::ImmReleaseContext(hWnd, hImc);
    }
    return TRUE;
}

BOOL WINAPI IME_Status_CreateCaret(
    __in HWND hWnd,
    __in_opt HBITMAP hBitmap,
    __in int nWidth,
    __in int nHeight) {
    // WriteLog(elDebug, TEXT("CreateCaret"));

    if (hBitmap) {
        BITMAP bmp = { 0 };
        ::GetObjectA(hBitmap, sizeof(bmp), &bmp);
        nWidth = bmp.bmWidth;
        nHeight = bmp.bmHeight;
    }

	HBITMAP hMemBitmap = hBitmap;
	HBITMAP hBmpForDelete = NULL;

	BOOL bRet = WinAPI_CreateCaret(hWnd, hMemBitmap, nWidth, nHeight);
	if (bRet) {
		InitCaretInfo(&g_CaretInfo);
		g_CaretInfo.bCreated = TRUE;
        g_CaretInfo.hBmp = hBmpForDelete;
        g_CaretInfo.args.hBmp = hBitmap;
        g_CaretInfo.args.width = nWidth;
        g_CaretInfo.args.height = nHeight;
	}
    else if (hBmpForDelete) {
		::DeleteObject(hBmpForDelete);
	}
	return bRet;
}

BOOL WINAPI IME_Status_ShowCaret(__in_opt HWND hWnd) {
    // WriteLog(elDebug, TEXT("ShowCaret"));
    BOOL bRet = WinAPI_ShowCaret(hWnd);
    if (bRet && hWnd) {
        ::InterlockedIncrement(&g_CaretInfo.lockedConut);
    }
    return bRet;
}

BOOL WINAPI IME_Status_HideCaret(__in_opt HWND hWnd) {
    // WriteLog(elDebug, TEXT("HideCaret"));
    BOOL bRet = WinAPI_HideCaret(hWnd);
    if (bRet && hWnd) {
        ::InterlockedDecrement(&g_CaretInfo.lockedConut);
    }
    return bRet;
}

BOOL WINAPI IME_Status_DestroyCaret() {
    // WriteLog(elDebug, TEXT("DestroyCaret"));

    BOOL bRet = WinAPI_DestroyCaret();
    return bRet;
}

BOOL WinAPI_HookManager::WinAPIHook() {
    MH_CreateHook(&CreateCaret, &IME_Status_CreateCaret,
        reinterpret_cast<LPVOID*>(&WinAPI_CreateCaret));
    MH_EnableHook(&CreateCaret);

    MH_CreateHook(&ShowCaret, &IME_Status_ShowCaret,
        reinterpret_cast<LPVOID*>(&WinAPI_ShowCaret));
    MH_EnableHook(&ShowCaret);

    MH_CreateHook(&HideCaret, &IME_Status_HideCaret,
        reinterpret_cast<LPVOID*>(&WinAPI_HideCaret));
    MH_EnableHook(&HideCaret);

    MH_CreateHook(&DestroyCaret, &IME_Status_DestroyCaret,
        reinterpret_cast<LPVOID*>(&WinAPI_DestroyCaret));
    MH_EnableHook(&DestroyCaret);

    return TRUE;
}

void WinAPI_HookManager::WinAPIUnHook() {
    MH_DisableHook(&CreateCaret);
    MH_DisableHook(&ShowCaret);
    MH_DisableHook(&HideCaret);
    MH_DisableHook(&DestroyCaret);
}

//---------------------------------------------------------------------------//

// DLL エントリポイント
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInstance;
    }
    return TRUE;
}
