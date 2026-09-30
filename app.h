#pragma once
#include <windows.h>

extern HINSTANCE g_hInstance;
extern HWND g_mainWindow;
extern HHOOK g_mouseHook;

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_SHOW_SETTINGS = WM_APP + 2;
constexpr UINT WM_CHANGE_VOLUME = WM_APP + 3;
constexpr UINT WM_AUDIO_DEVICE_CHANGED = WM_APP + 4;

constexpr int CMD_TRAY_SETTINGS = 1001;
constexpr int CMD_TRAY_EXIT = 1002;

HICON LoadAppIcon();
