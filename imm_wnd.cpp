#include "tstring.hpp"
#include "imm_wnd.h"

HINSTANCE g_hImmInst = nullptr;
HWND      g_hImmWnd  = nullptr;

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

HWND ControlExist(HWND aParentWindow, LPTSTR aClassNameAndNum)
{
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

BOOL ControlGetFocus(LPTSTR aTitle, LPTSTR aText) {
    Var& output_var = *OUTPUT_VAR; // Must be resolved only once and prior to DetermineTargetWindow().  See Line::WinGetClass() for explanation.
    output_var.Assign();  // Set default: blank for the output variable.
    HWND target_window = GetForegroundWindow();
    if (!target_window) { return FALSE; }

    GUITHREADINFO guithreadInfo;
    guithreadInfo.cbSize = sizeof(GUITHREADINFO);
    if (!GetGUIThreadInfo(GetWindowThreadProcessId(target_window, NULL), &guithreadInfo)) {
        return FALSE;
    }

    class_and_hwnd_type cah;
    TCHAR class_name[WINDOW_CLASS_SIZE];
    cah.hwnd = guithreadInfo.hwndFocus;
    cah.class_name = class_name;
    if (!GetClassName(cah.hwnd, class_name, _countof(class_name) - 5)) {
        return FALSE;
    }

    cah.class_count = 0;
    cah.is_found = false;
    EnumChildWindows(target_window, EnumChildFindSeqNum, (LPARAM)&cah);
    if (!cah.is_found) { return FALSE; }
    // Append the class sequence number onto the class name set the output param to be that value:
    sntprintfcat(class_name, _countof(class_name), _T("%d"), cah.class_count);
    g_ErrorLevel->Assign(ERRORLEVEL_NONE); // Indicate success.
    return output_var.Assign(class_name);
}

int WINAPI GenerateImmWindow(HINSTANCE hInstance) {
    WNDCLASS winc;
    MSG msg;

    g_hImmInst = hInstance;

    winc.style         = CS_HREDRAW | CS_VREDRAW;
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
        200,
        200,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (g_hImmWnd == NULL) { return 0; }

    while (GetMessage(&msg, NULL, 0, 0)) {
        DispatchMessage(&msg);
    }

    UnregisterClass(TEXT("ClipBoardExtClass"), hInstance);

    return (int)msg.wParam;
};