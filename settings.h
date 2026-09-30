#pragma once
#include <windows.h>

bool IsStartWithWindowsEnabled();
bool SetStartWithWindows(bool enabled);

bool LoadOverlaySetting();
void SaveOverlaySetting(bool enabled);

void ShowSettingsWindow(HWND owner);
