#pragma once

#define _CRT_SECURE_DEPRECATE_MEMORY
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#if defined(UNICODE) || defined(_UNICODE)
#define tmemcpy			wmemcpy
#define tmemmove		wmemmove
#define tmemset			wmemset
#define tmemcmp			wmemcmp
#define tmalloc(c)		 ((TCHAR *) malloc((c) << 1))
#define trealloc(p, c) ((TCHAR *) realloc((p), (c) << 1))
#define talloca(c)		 ((TCHAR *) _alloca((c) << 1))
#define tcslcpy         wcslcpy
#else
#define tmemcpy			(char*)memcpy
#define tmemmove		memmove
#define tmemset			memset
#define tmemcmp			memcmp
#define tmalloc(c)		((TCHAR *) malloc(c))
#define trealloc(p, c)	((TCHAR *) realloc((p), (c)))
#define talloca(c)		((TCHAR *) _alloca(c))
#define tcslcpy         strlcpy
#endif

void strlcpy(LPSTR aDst, LPCSTR aSrc, size_t aDstSize) {
	--aDstSize;
	strncpy(aDst, aSrc, aDstSize);
	aDst[aDstSize] = '\0';
}

void wcslcpy(LPWSTR aDst, LPCWSTR aSrc, size_t aDstSize) {
	--aDstSize;
	wcsncpy(aDst, aSrc, aDstSize);
	aDst[aDstSize] = '\0';
}

int sntprintfcat(LPTSTR aBuf, int aBufSize, LPCTSTR aFormat, ...) {
	size_t length = _tcslen(aBuf);
	int space_remaining = (int)(aBufSize - length);
	if (space_remaining < 1) {
		return 0;
	}
	aBuf += length;
	va_list ap;
	va_start(ap, aFormat);
	int result = _vsntprintf(aBuf, (size_t)space_remaining, aFormat, ap); // "returns the number of characters written, not including the terminating null character, or a negative value if an output error occurs"
	aBuf[space_remaining - 1] = '\0'; // Confirmed through testing: Must terminate at this exact spot because _vsnprintf() doesn't always do it.
	return result > -1 ? result : space_remaining - 1; // Never return a negative value.  See comment under function definition, above.
}