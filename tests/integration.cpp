#include "../main.cpp"
#include "../audio.cpp"
#include "../overlay.cpp"
#include "../tray.cpp"
#include "../settings.cpp"
#include <cstdio>
#include <stdexcept>

static int checks = 0;
static void Check(bool condition, const char* name)
{
    if (!condition) { std::printf("FAIL: %s\n", name); throw std::runtime_error(name); }
    ++checks;
    std::printf("PASS: %s\n", name);
}
static void Pump(unsigned duration)
{
    ULONGLONG end = GetTickCount64() + duration;
    do {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        Sleep(5);
    } while (GetTickCount64() < end);
}
static int ReadVolume()
{
    float scalar = 0;
    Check(g_endpointVolume && SUCCEEDED(g_endpointVolume->GetMasterVolumeLevelScalar(&scalar)), "read actual volume");
    return static_cast<int>(std::round(scalar * 100));
}
static int RunTests()
{
    g_hInstance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"TaskbarVolumeTestWindow";
    Check(RegisterClassW(&wc) != 0, "register hidden test window");
    g_mainWindow = CreateWindowW(wc.lpszClassName, L"", 0, 0,0,0,0, nullptr,nullptr,g_hInstance,nullptr);
    Check(g_mainWindow != nullptr, "create broadcast-capable window");
    Check(InitAudio(), "initialize audio and device notifications");
    Check(g_endpointVolume != nullptr, "bind default output device");
    float original = 0;
    BOOL muted = FALSE;
    Check(SUCCEEDED(g_endpointVolume->GetMasterVolumeLevelScalar(&original)), "save volume");
    Check(SUCCEEDED(g_endpointVolume->GetMute(&muted)), "save mute state");
    struct Restore {
        float scalar; BOOL mute;
        ~Restore() { if (g_endpointVolume) { g_endpointVolume->SetMasterVolumeLevelScalar(scalar,nullptr); g_endpointVolume->SetMute(mute,nullptr); } }
    } restore{original, muted};
    Check(CreateOverlayWindow(g_hInstance), "create overlay");
    SetOverlayEnabled(false);
    Check(SUCCEEDED(g_endpointVolume->SetMasterVolumeLevelScalar(.50f,nullptr)), "set test baseline");
    ChangeVolume(1); Check(ReadVolume() == 51, "up one notch adds 1 percent");
    ChangeVolume(1); Check(ReadVolume() == 52, "odd volume increments without snapping");
    ChangeVolume(-1); Check(ReadVolume() == 51, "down one notch subtracts 1 percent");
    ChangeVolume(-1); Check(ReadVolume() == 50, "odd volume decrements without snapping");
    ChangeVolume(1000); Check(ReadVolume() == 100, "upper boundary");
    ChangeVolume(-1000); Check(ReadVolume() == 0, "lower boundary");
    BOOL nowMuted = FALSE;
    Check(SUCCEEDED(g_endpointVolume->GetMute(&nowMuted)) && nowMuted, "zero mutes");
    ChangeVolume(1);
    Check(SUCCEEDED(g_endpointVolume->GetMute(&nowMuted)) && !nowMuted, "up unmutes");
    g_endpointVolume->SetMasterVolumeLevelScalar(.50f,nullptr);
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    RECT rc{};
    Check(taskbar && GetWindowRect(taskbar, &rc), "locate real taskbar");
    POINT pt{(rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2};
    Check(IsTaskbarWindow(WindowFromPoint(pt)), "recognize logical taskbar coordinates at current scaling");
    POINT cursor{};
    Check(GetCursorPos(&cursor) && IsCursorOverTaskbar() == IsTaskbarWindow(WindowFromPoint(cursor)),
          "cursor detection uses GetCursorPos coordinate space");
    Check(QueueTaskbarWheel(60), "accept partial wheel event");
    Check(ReadVolume() == 50 && g_wheelRemainder == 60, "partial wheel does not change volume");
    QueueTaskbarWheel(60);
    Check(ReadVolume() == 50, "hook defers audio work");
    Pump(20); Check(ReadVolume() == 51, "two partial events form one notch");
    QueueTaskbarWheel(-240);
    Pump(20); Check(ReadVolume() == 49, "negative multi-notch message keeps sign");
    g_notifications->OnDefaultDeviceChanged(eRender, eMultimedia, nullptr);
    Pump(20); Check(g_endpointVolume != nullptr, "device notification rebinds on message loop");
    g_endpointVolume->SetMasterVolumeLevelScalar(original, nullptr);
    g_endpointVolume->SetMute(muted, nullptr);
    SetOverlayEnabled(true);
    ShowVolumeOverlay(48); Pump(20);
    DWORD baseline = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 300; ++i) { ShowVolumeOverlay(i % 101); UpdateWindow(g_overlayWindow); }
    Check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == baseline, "300 redraws keep GDI count stable");
    Pump(500); ShowVolumeOverlay(50); Pump(500);
    Check(IsWindowVisible(g_overlayWindow) != FALSE, "new activity resets hide timer");
    Pump(600); Check(!IsWindowVisible(g_overlayWindow), "overlay hides after inactivity");
    ShowVolumeOverlay(50); SetOverlayEnabled(false);
    Pump(1000); Check(!IsWindowVisible(g_overlayWindow), "disabled overlay stays hidden");
    ShowSettingsWindow(g_mainWindow);
    Check(g_settingsWindow != nullptr, "open settings");
    SendMessageW(g_settingsWindow, WM_CLOSE, 0, 0);
    Check(!g_settingsWindow, "close settings releases window");
    ShowSettingsWindow(g_mainWindow);
    Check(g_settingsWindow != nullptr, "reopen settings");
    DestroyWindow(g_settingsWindow);
    RemoveTrayIcon(g_mainWindow);
    Check(AddTrayIcon(g_mainWindow), "add tray icon");
    RemoveTrayIcon(g_mainWindow);
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    SendMessageW(g_mainWindow, g_taskbarCreated, 0, 0);
    NOTIFYICONDATAW nid{}; nid.cbSize = sizeof(nid); nid.hWnd = g_mainWindow; nid.uID = 1;
    Check(Shell_NotifyIconW(NIM_MODIFY, &nid) != FALSE, "taskbar recreation message restored icon");
    RemoveTrayIcon(g_mainWindow);
    DestroyOverlayWindow();
    Check(!g_overlayWindow && !g_font && !g_background, "release overlay resources");
    CleanupAudio();
    Check(!g_endpointVolume && !g_enumerator && !g_notifications && !g_comInitialized, "release audio and COM resources");
    DestroyWindow(g_mainWindow);
    std::printf("All %d checks passed. Original volume and mute state restored.\n", checks);
    return 0;
}

int main() { try { return RunTests(); } catch (const std::exception& error) { std::printf("FAIL: %s\n", error.what()); return 1; } }

