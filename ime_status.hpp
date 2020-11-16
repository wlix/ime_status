#pragma once

#include <SDKDDKVer.h>

#include <windows.h>

#define IMC_GETOPENSTATUS     0x0005
#define WM_SHOWCARETIFNEED    WM_APP + 1

typedef struct _CONFIG_DATA {
    UINT on;
    UINT off;
} CONFIG_DATA;

