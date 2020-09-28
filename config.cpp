#include "config.hpp"
#include "Utility.hpp"

#include <strsafe.h>

LPCWSTR kMutexName = L"IMEStatusMutex";

extern HINSTANCE    g_hInst;
extern HANDLE       g_hShared;
extern SHARED_DATA* g_Shared;

BOOL WritePrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, int value, LPCWSTR lpFileName) {
	wchar_t szbuff[32];
	wsprintfW(szbuff, L"%d", value);
	return ::WritePrivateProfileStringW(lpAppName, lpKeyName, szbuff, lpFileName);
}

config::config() : m_hMutex(NULL) {
	m_hMutex = ::CreateMutexW(nullptr, FALSE, kMutexName);
	_ASSERT(m_hMutex);
}

config::~config() {
	::CloseHandle(m_hMutex);
}

config& config::get_instance() {
	static config s_instance;
	return s_instance;
}

void config::load_config() {
	mutex_locker lock(m_hMutex);

	wchar_t inipath[MAX_PATH];
	size_t len = ::GetModuleFileNameW(g_hInst, inipath, MAX_PATH);

	if (len < 4) {
		return;
	} else {
		inipath[len - 1] = L'i';
		inipath[len - 2] = L'n';
		inipath[len - 3] = L'i';
	}

#if defined(_WIN64) || defined(WIN64)
	// g_Shared.x64_inipath = inipath;
	::StringCchCopyW(g_Shared->x64_inipath, MAX_PATH, inipath);
#else
	// g_Shared.x86_inipath = inipath;
	::StringCchCopyW(g_Shared->x86_inipath, MAX_PATH, inipath);
#endif

	g_Shared->on = ::GetPrivateProfileIntW(L"Setting", L"On", 100, inipath);
	g_Shared->off = ::GetPrivateProfileIntW(L"Setting", L"Off", 500, inipath);
}
