#include "config.hpp"
#include "Utility.hpp"

#include <strsafe.h>

extern HINSTANCE    g_hInst;
extern HANDLE       g_hShared;
extern CONFIG_DATA  g_Config;

BOOL WritePrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int value, LPCTSTR lpFileName) {
	TCHAR szbuff[32];
	wsprintf(szbuff, TEXT("%d"), value);
	return ::WritePrivateProfileString(lpAppName, lpKeyName, szbuff, lpFileName);
}

config::config() {
}

config::~config() {
}

config& config::get_instance() {
	static config s_instance;
	return s_instance;
}

void config::load_config() {
	TCHAR inipath[MAX_PATH];
	size_t len = ::GetModuleFileName(g_hInst, inipath, MAX_PATH);

	if (len < 4) {
		return;
	}
	else {
		inipath[len - 1] = L'i';
		inipath[len - 2] = L'n';
		inipath[len - 3] = L'i';
	}

	g_Config.on = ::GetPrivateProfileInt(TEXT("Setting"), TEXT("On"), 200, inipath);
	g_Config.off = ::GetPrivateProfileInt(TEXT("Setting"), TEXT("Off"), 530, inipath);
}
