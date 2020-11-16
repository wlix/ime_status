#include "pch.h"
#include <imm.h>
#include <atlwin.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
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
BOOL GetConversionStatus(HWND hWnd, DWORD* pConversion);
void FocusChangedHandler(IUIAutomationElement* sender);

enum { kCaretWidth = 2, kCaretRightPadding = 2 };

#pragma data_seg(".SHARED_DATA")
HHOOK  g_hHook = nullptr;
#pragma data_seg()

CAppModule _Module;

#if defined(WIN64) || defined(_WIN64)
LPCTSTR CCaretWndClassName = TEXT("_CaretWndForIMEStatus_x64_");
#else
LPCTSTR CCaretWndClassName = TEXT("_CaretWndForIMEStatus_x86_");
#endif

HINSTANCE   g_hInst       = nullptr;
CONFIG_DATA g_Config      = {};
BOOL        g_bAPIHooked  = FALSE;
LONG        g_lockedConut = 0;
HWND		g_wnd30C4     = NULL;

/*class CCaretWnd : public CDoubleBufferWindowImpl<
    CCaretWnd, CWindow, CWinTraits<WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST>>*/
class CCaretWnd : public CFrameWindowImpl<CCaretWnd, CWindow, CWinTraits<WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST>> {

public:
    DECLARE_WND_CLASS(CCaretWndClassName)

    const int TimerId = 1;
    const int TimerInterval = 100;

    void SetSender(IUIAutomationElement* sender) {
        m_sp_sender = sender;
        if (sender) {
            SetTimer(TimerId, TimerInterval);
            SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
        }
        else {
            KillTimer(TimerId);
            ShowWindow(SW_HIDE);
        }
    }

    void ChangeIMEState(BOOL cond) {
        if (m_ime_on != cond) {
            m_ime_on = cond;
            if (m_ime_on) {
                SetCaretBlinkTime(g_Config.on);
            }
            else {
                SetCaretBlinkTime(g_Config.off);
            }
            Invalidate(FALSE);
        }
    }

    BEGIN_MSG_MAP_EX(CCaretWnd)
        MSG_WM_TIMER(OnTimer)
        MESSAGE_HANDLER_EX(WM_SHOWCARETIFNEED, OnShowCaretIfNeed)
        CHAIN_MSG_MAP(__super)
    END_MSG_MAP()

    LRESULT OnShowCaretIfNeed(UINT, WPARAM wParam, LPARAM lParam) {
        if (lParam != 0) {
            SetSender(nullptr);
            return 0;
        }
        ChangeIMEState(wParam != 0);
        return 0;
    }

    /*void DoPaint(CDCHandle dc) {
        RECT rc;
        GetClientRect(&rc);
        if (m_ime_on) {
            dc.FillSolidRect(&rc, 0x00000000);
        }
        else {
            dc.FillSolidRect(&rc, 0x00000000);
        }
    }*/

    void OnTimer(UINT_PTR nIDEvent) {
        if (nIDEvent != TimerId) { return; }

        HWND hForeWnd = ::GetForegroundWindow();
        CRect rcForeground;
        ::GetWindowRect(hForeWnd, &rcForeground);

        CRect rcBound;
        CComPtr<IUIAutomationElement> spSender = m_sp_sender;
        if (spSender) {
            spSender->get_CurrentBoundingRectangle(&rcBound);

            CRect rcCaret = rcBound;
            rcCaret.left -= kCaretWidth + kCaretRightPadding;
            rcCaret.right = rcBound.left - kCaretRightPadding;
            MoveWindow(rcCaret);
        }

        DWORD threadId = ::GetWindowThreadProcessId(hForeWnd, nullptr);
        GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
        ::GetGUIThreadInfo(threadId, &gti);
        HWND HImeWnd = ::ImmGetDefaultIMEWnd(gti.hwndFocus);
        LRESULT ret = ::SendMessage(HImeWnd, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0);
        ChangeIMEState(ret != 0);

        if ((rcForeground.PtInRect(rcBound.TopLeft()) == FALSE &&
            rcForeground.PtInRect(rcBound.BottomRight()) == FALSE)) {
            ShowWindow(SW_HIDE);
        }
        else {
            ShowWindow(SW_SHOWNOACTIVATE);
        }
    }

private:
    CComPtr<IUIAutomationElement> m_sp_sender;
    BOOL m_ime_on = FALSE;
};

class WinAPI_HookManager {
public:
    WinAPI_HookManager() {
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

/*private:
    HMODULE m_hHookDll;*/
};

CCaretWnd   g_CaretWnd;

typedef struct _CREATE_CARET_ARGS {
    HBITMAP hBmp = NULL;
    int     width = 0;
    int     height = 0;
} CREATE_CARET_ARGS;

typedef struct _CARET_INFO {
    BOOL              bCreated = FALSE;
    HBITMAP           hBmp = NULL;
    CREATE_CARET_ARGS args;
} CARET_INFO;

CARET_INFO        g_CaretInfo;

typedef BOOL (WINAPI* pfCreateCaret) (HWND, HBITMAP, int, int);
typedef BOOL (WINAPI* pfShowCaret)   (HWND);
typedef BOOL (WINAPI* pfHideCaret)   (HWND);

pfCreateCaret  WinAPI_CreateCaret  = NULL;
pfShowCaret    WinAPI_ShowCaret    = NULL;
pfHideCaret    WinAPI_HideCaret    = NULL;

std::unique_ptr<CUIAutomationClient> g_ui_automation;

// プラグインの名前
#if defined(WIN64) || defined(_WIN64)
LPCTSTR PLUGIN_NAME  { TEXT("IME Status for Win10 x64") };
#else
LPCTSTR PLUGIN_NAME  { TEXT("IME Status for Win10 x86") };
#endif

// コマンドの数
DWORD COMMAND_COUNT { 0 };

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

// --------------------------------------------------------
//    フックプロシージャ
// --------------------------------------------------------
LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        LPCWPSTRUCT pcw = (LPCWPSTRUCT)lParam;
        if (pcw->message == WM_IME_NOTIFY && pcw->wParam == IMN_SETOPENSTATUS) {
            WriteLog(elDebug, TEXT("Hook SetOpenStatus"));
            LONG Count = g_lockedConut;
            HWND wnd30C4 = g_wnd30C4;
            int Width = g_CaretInfo.args.width;
            int Height = g_CaretInfo.args.height;
            WriteLog(elDebug, TEXT("g_wnd30C4: %d"), g_wnd30C4);

            POINT pt;
            ::GetCaretPos(&pt);
            ::DestroyCaret();
            ::CreateCaret(wnd30C4, NULL, Width, Height);
            WriteLog(elDebug, TEXT("CreateCaret: wnd30C4: %d, Width: %d, Height: %d"), wnd30C4, Width, Height);
            ::SetCaretPos(pt.x, pt.y);

            HWND HCaretWnd = ::FindWindow(CCaretWndClassName, nullptr);
            WriteLog(elDebug, TEXT("FindWindow"));
            if (HCaretWnd) {
                WriteLog(elDebug, TEXT("Detected CCaretWndClass: %08x"), HCaretWnd);
                WriteLog(elDebug, TEXT("PostMessage: hwnd %08x, IsImeEnabled %d"), wnd30C4 ? wnd30C4 : pcw->hwnd, IsImeEnabled(wnd30C4 ? wnd30C4 : pcw->hwnd));
                ::PostMessage(HCaretWnd, WM_SHOWCARETIFNEED, IsImeEnabled(wnd30C4 ? wnd30C4 : pcw->hwnd), 0);
            }

            WriteLog(elDebug, TEXT("Count: %d"), Count);
            if (Count > 0) {
                for (int i = 0; i < Count; ++i) {
                    ::ShowCaret(wnd30C4);
                }
            }
            else if (Count < 0) {
                for (LONG i = 0; i > Count; --i) {
                    ::HideCaret(wnd30C4);
                }
            }
        }
        else if (pcw->message == WM_IME_SETCONTEXT && pcw->wParam == FALSE) {
            WriteLog(elDebug, TEXT("Hook SetContext"));
            HIMC hContext = ::ImmGetContext(pcw->hwnd);

            BOOL hideCaret = (hContext == NULL);
            HWND HCaretWnd = ::FindWindow(CCaretWndClassName, nullptr);
            WriteLog(elDebug, TEXT("Findow Window: %s"), CCaretWndClassName);
            if (HCaretWnd && hideCaret) {
                WriteLog(elDebug, TEXT("PostMessage: WM_SHOWCARETIFNEED"));
                ::PostMessage(HCaretWnd, WM_SHOWCARETIFNEED, 0, hideCaret);
            }
        } else if (pcw->message == WM_SETFOCUS) {
            WriteLog(elDebug, TEXT("Hook WM_SETFOCUS"));
            static WinAPI_HookManager mng;
        }
    }
    return ::CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

void FocusChangedHandler(IUIAutomationElement* sender) {
    WriteLog(elDebug, TEXT("Begin FocusChangedHandler sender: %08x"), sender);
    CONTROLTYPEID	controlTypeId = 0;
    sender->get_CurrentControlType(&controlTypeId);
    WriteLog(elDebug, TEXT("TypeId: %d"), controlTypeId);
    if (controlTypeId != UIA_EditControlTypeId &&
        controlTypeId != UIA_ComboBoxControlTypeId &&
        controlTypeId != UIA_CustomControlTypeId) {
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
                g_ui_automation->GetUIAutomation()->CreatePropertyCondition(
                    UIA_ControlTypePropertyId, vControlType, &spCndEdit);
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
        if (hEditWnd) {
            TCHAR className[128] = TEXT("");
            ::GetClassName(hEditWnd, className, 128);
            if (::_tcscmp(className, TEXT("Edit")) == 0) {
                g_CaretWnd.SetSender(nullptr);
                return;
            }
        }

        CComPtr<IUIAutomationValuePattern> spValuePattern;
        sender->GetCurrentPatternAs(UIA_ValuePatternId,
            IID_IUIAutomationValuePattern, (void**)&spValuePattern);
        if (spValuePattern) {
            BOOL bIsReadOnly = FALSE;
            spValuePattern->get_CurrentIsReadOnly(&bIsReadOnly);
            if (bIsReadOnly) {
                g_CaretWnd.SetSender(nullptr);
                return;
            }
        }
    }

    HWND hForeWnd = ::GetForegroundWindow();
    DWORD threadId = ::GetWindowThreadProcessId(hForeWnd, nullptr);
    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    ::GetGUIThreadInfo(threadId, &gti);

    if (gti.flags == GUI_CARETBLINKING && hEditWnd) {
        g_CaretWnd.SetSender(nullptr);
        return;
    }

    CRect rcBounds;
    sender->get_CurrentBoundingRectangle(&rcBounds);
    rcBounds.left -= kCaretWidth + kCaretRightPadding;
    rcBounds.right = rcBounds.left - kCaretRightPadding;
    g_CaretWnd.MoveWindow(rcBounds);

    HWND hIMEWnd = ::ImmGetDefaultIMEWnd(gti.hwndFocus);
    BOOL ret = ::SendMessage(hIMEWnd, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0);
    g_CaretWnd.ChangeIMEState(ret != 0);

    g_CaretWnd.SetSender(sender);
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

    SYSTEM_INFO sysInfo = {};
    GetNativeSystemInfo(&sysInfo);

    BOOL bIsWow64 = FALSE;
    ATLVERIFY(IsWow64Process(GetCurrentProcess(), &bIsWow64));

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
    ::UnhookWindowsHookEx(g_hHook);
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
BOOL WINAPI Execute(INT32 CmdId, HWND hWnd) {
    return TRUE;
}

//---------------------------------------------------------------------------//

// TTBEvent_WindowsHook() の内部実装
void WINAPI Hook(UINT Msg, WPARAM wParam, LPARAM lParam) {
}

void InitCaretInfo() {
	if (g_CaretInfo.hBmp) {
		::DeleteObject(g_CaretInfo.hBmp);
        g_CaretInfo.hBmp = NULL;
	}
    g_lockedConut = 0;
    g_CaretInfo.bCreated = FALSE;
    if (g_CaretInfo.args.hBmp) {
        ::DeleteObject(g_CaretInfo.args.hBmp);
        g_CaretInfo.hBmp = NULL;
    }
    g_CaretInfo.args.width = 0;
    g_CaretInfo.args.height = 0;
}

BOOL IsImeEnabled(HWND hWnd) {
    DWORD Conversion = 0;
    GetConversionStatus(hWnd, &Conversion);
    if (Conversion == IME_CMODE_ALPHANUMERIC
        || Conversion == IME_CMODE_NOCONVERSION ) {
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
	HBITMAP hMemBitmap = hBitmap;
	HBITMAP hBmpForDelete = NULL;

	HDC hDC = ::GetDC(hWnd);
	if (hDC) {
		HDC hMemDC = ::CreateCompatibleDC(hDC);
		if (hMemDC) {
			nWidth = 2;
			COLORREF color = 0x00000000;
			DWORD Conversion = 0;
			if (GetConversionStatus(hWnd, &Conversion)) {
				if (Conversion == IME_CMODE_ALPHANUMERIC
                    || Conversion == IME_CMODE_NOCONVERSION) {
					nWidth = 2;
					color = 0x00000000;
				}
			}

			hMemBitmap = ::CreateCompatibleBitmap(hDC, nWidth, nHeight);
			hBmpForDelete = hMemBitmap;
			if (hMemBitmap) {
				color = ~color;
				color &= 0x00FFFFFF;
				HBRUSH hbr = ::CreateSolidBrush(color);
				if (hbr) {
					HGDIOBJ hPrevBitmap = ::SelectObject(hMemDC, hMemBitmap);
					RECT rc;
					rc.top = 0;
					rc.left = 0;
					rc.right = nWidth;
					rc.bottom = nHeight;
					::FillRect(hMemDC, &rc, hbr);
					::SelectObject(hMemDC, hPrevBitmap);
					::DeleteObject(hbr);
				}
			}
			::DeleteDC(hMemDC);
		}
		::ReleaseDC(hWnd, hDC);
	}

	BOOL bRet = WinAPI_CreateCaret(hWnd, hMemBitmap, nWidth, nHeight);
	if (bRet) {
		InitCaretInfo();
		g_wnd30C4 = hWnd;
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
    BOOL bRet = WinAPI_ShowCaret(hWnd);
    if (bRet && hWnd && hWnd == g_wnd30C4) {
        ::InterlockedIncrement(&g_lockedConut);
    }
    return bRet;
}

BOOL WINAPI IME_Status_HideCaret(__in_opt HWND hWnd) {
    BOOL bRet = WinAPI_HideCaret(hWnd);
    if (bRet && hWnd && hWnd == g_wnd30C4) {
        ::InterlockedDecrement(&g_lockedConut);
    }
    return bRet;
}

BOOL WinAPI_HookManager::WinAPIHook() {
    MH_CreateHook(static_cast<void*>(&CreateCaret),
        static_cast<void*>(&IME_Status_CreateCaret),
        reinterpret_cast<void**>(&WinAPI_CreateCaret));
    MH_EnableHook(&CreateCaret);

    MH_CreateHook(static_cast<void*>(&ShowCaret),
        static_cast<void*>(&IME_Status_ShowCaret),
        reinterpret_cast<void**>(&WinAPI_ShowCaret));
    MH_EnableHook(&ShowCaret);

    MH_CreateHook(static_cast<void*>(&HideCaret),
        static_cast<void*>(&IME_Status_HideCaret),
        reinterpret_cast<void**>(&WinAPI_HideCaret));
    MH_EnableHook(&HideCaret);

    return TRUE;
}

void WinAPI_HookManager::WinAPIUnHook() {
    MH_DisableHook(&CreateCaret);
    MH_DisableHook(&ShowCaret);
    MH_DisableHook(&HideCaret);
}

//---------------------------------------------------------------------------//

// DLL エントリポイント
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInstance;
    }
    return TRUE;
}
