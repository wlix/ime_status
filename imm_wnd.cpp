#include "tstring.hpp"
#include "Utility.hpp"

#include "config.hpp"
#include "ime_status.hpp"

#include "imm_wnd.h"

CONFIG_DATA g_Config;

HINSTANCE g_hImmInst = nullptr;
HWND      g_hImmWnd  = nullptr;

HWND                hwnd_focus = nullptr;
class_and_hwnd_type cah_focus;
TCHAR               class_name_focus[WINDOW_CLASS_SIZE] = { '\0' };
HWND                hwnd_pre_focus = nullptr;
class_and_hwnd_type cah_pre_focus;
TCHAR               class_name_pre_focus[WINDOW_CLASS_SIZE] = { '\0' };

HWND GetTopChild(HWND aParent) {
    if (!aParent) return aParent;
    HWND hwnd_top, next_top;
    for (hwnd_top = GetTopWindow(aParent)
        ; hwnd_top && (next_top = GetTopWindow(hwnd_top))
        ; hwnd_top = next_top);

    return hwnd_top ? hwnd_top : aParent;
}

BOOL CALLBACK EnumControlFind(HWND aWnd, LPARAM lParam) {
	WindowSearch& ws = *(WindowSearch*)lParam;
	if (*ws.mCriterionClass) {
		int length = GetClassName(aWnd, ws.mCandidateTitle, WINDOW_CLASS_SIZE); // Restrict the length to a small fraction of the buffer's size (also serves to leave room to append the sequence number).

		if (length && !_tcsnicmp(ws.mCriterionClass, ws.mCandidateTitle, length)) {
			_itot(++ws.mAlreadyVisitedCount, ws.mCandidateTitle, 10);
			if (!_tcsicmp(ws.mCandidateTitle, ws.mCriterionClass + length)) {
				ws.mFoundChild = aWnd;
				return FALSE;
			}
		}
	}
	return TRUE;
}

HWND ControlExist(HWND aParentWindow, LPTSTR aClassNameAndNum) {
    if (!aParentWindow)
        return NULL;
    if (!aClassNameAndNum || !*aClassNameAndNum)
        return (GetWindowLongPtr(aParentWindow, GWL_STYLE) & WS_CHILD) ? aParentWindow : GetTopChild(aParentWindow);

    WindowSearch ws;
    bool is_class_name = _istdigit(aClassNameAndNum[_tcslen(aClassNameAndNum) - 1]);

    if (is_class_name) {
        tcslcpy(ws.mCriterionClass, aClassNameAndNum, _countof(ws.mCriterionClass));
        ws.mCriterionText = _T("");
    }
    else {
        return NULL;
    }
    EnumChildWindows(aParentWindow, EnumControlFind, (LPARAM)&ws);
    return ws.mFoundChild;
}

BOOL CALLBACK EnumChildFindSeqNum(HWND aWnd, LPARAM lParam) {
    class_and_hwnd_type& cah = *(class_and_hwnd_type*)lParam;
    TCHAR class_name[WINDOW_CLASS_SIZE];
    if (!GetClassName(aWnd, class_name, _countof(class_name))) {
        return TRUE;
    }
    if (!_tcscmp(class_name, cah.class_name)) {
        ++cah.class_count;
        if (aWnd == cah.hwnd) {
            cah.is_found = true;
            return FALSE;
        }
    }
    return TRUE;
}

BOOL ControlGetFocus() {
    hwnd_focus = GetForegroundWindow();
    if (!hwnd_focus) { return FALSE; }

    GUITHREADINFO guithreadInfo;
    guithreadInfo.cbSize = sizeof(GUITHREADINFO);
    if (!GetGUIThreadInfo(GetWindowThreadProcessId(hwnd_focus, NULL), &guithreadInfo)) {
        return FALSE;
    }

    cah_focus.hwnd = guithreadInfo.hwndFocus;
    cah_focus.class_name = class_name_focus;
    if (!GetClassName(cah_focus.hwnd, class_name_focus, _countof(class_name_focus) - 5)) {
        return FALSE;
    }

    // cah_focus.class_count = 0;
    // cah_focus.is_found = false;
    // EnumChildWindows(hwnd_focus, EnumChildFindSeqNum, (LPARAM)&cah_focus);
    /*if (!cah_focus.is_found) { return FALSE; }
    sntprintfcat(class_name_focus, _countof(class_name_focus), _T("%d"), cah_focus.class_count);*/
    return TRUE;
}

LRESULT CALLBACK ImmProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        config::get_instance().load_config();
        return 0;
    case IMM_FOCUS_AND_OPENSTATUS:
        if (ControlGetFocus()) {
            hwnd_pre_focus = hwnd_focus;
            cah_pre_focus = cah_focus;
            tmemcpy(class_name_pre_focus, class_name_focus, WINDOW_CLASS_SIZE);

            HIMC hIMC = ImmGetContext(hwnd_focus);
            if (hIMC) {
                if (SendMessage(ImmGetDefaultIMEWnd(hwnd_focus), WM_IME_CONTROL, IMC_GETOPENSTATUS, 0)) {
                    WriteLog(elDebug, TEXT("ON: %d"), g_Config.on);
                    SetCaretBlinkTime(g_Config.on);
                }
                else {
                    WriteLog(elDebug, TEXT("OFF: %d"), g_Config.off);
                    SetCaretBlinkTime(g_Config.off);
                }
                ImmReleaseContext(hwnd_focus, hIMC);
            }
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI GenerateImmWindow(HINSTANCE hInstance) {
    WNDCLASS winc;
    MSG msg;

    g_hImmInst = hInstance;

    winc.style         = 0;
    winc.lpfnWndProc   = ImmProc;
    winc.cbClsExtra    = winc.cbWndExtra = 0;
    winc.hInstance     = hInstance;
    winc.hIcon         = NULL;
    winc.hCursor       = NULL;
    winc.hbrBackground = NULL;
    winc.lpszMenuName  = NULL;
    winc.lpszClassName = TEXT("IMEStatusClass");

    if (!RegisterClass(&winc)) return 0;

    g_hImmWnd = CreateWindow(
        TEXT("IMEStatusClass"),
        TEXT("IMEStatus"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        HWND_MESSAGE,
        NULL,
        hInstance,
        NULL);

    if (g_hImmWnd == NULL) { return 0; }

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClass(TEXT("IMEStatusClass"), hInstance);

    return (int)msg.wParam;
};