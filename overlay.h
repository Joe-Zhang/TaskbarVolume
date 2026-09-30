#pragma once
#include <windows.h>

bool CreateOverlayWindow(HINSTANCE hInstance);
void ShowVolumeOverlay(int volume);
void SetOverlayEnabled(bool enabled);
bool GetOverlayEnabled();
void DestroyOverlayWindow();
