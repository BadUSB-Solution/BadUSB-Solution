#pragma once

#include <windows.h>

// Tray
static const UINT WM_TRAYICON = WM_APP + 100;
static const UINT TRAY_ICON_ID = 1;
static const UINT IDM_TRAY_OPEN = 40001;
static const UINT IDM_TRAY_EXIT = 40002;

// Logs notify
#define WM_APP_NEWLOG (WM_APP + 1)