#include "app.h"
#include "audio.h"
#include "overlay.h"
#include "settings.h"
#include "tray.h"

#include <windows.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")

HINSTANCE g_hInstance = nullptr;
HWND g_mainWindow = nullptr;
HHOOK g_mouseHook = nullptr;

static HANDLE g_singleInstanceMutex = nullptr;
static UINT g_taskbarCreated = 0;
static int g_wheelRemainder = 0;

HICON LoadAppIcon()
{
    wchar_t systemDir[MAX_PATH]{};
    GetSystemDirectoryW(systemDir, MAX_PATH);

    std::wstring path =
        std::wstring(systemDir) + L"\\mmsys.cpl";

    HICON largeIcon = nullptr;
    HICON smallIcon = nullptr;

    UINT count = ExtractIconExW(
        path.c_str(),
        0,
        &largeIcon,
        &smallIcon,
        1
    );

    if (count > 0)
    {
        if (largeIcon)
        {
            if (smallIcon)
                DestroyIcon(smallIcon);
            return largeIcon;
        }

        if (smallIcon)
            return smallIcon;
    }

    return CopyIcon(
        LoadIconW(
            nullptr,
            MAKEINTRESOURCEW(32512)
        )
    );
}

static bool IsTaskbarWindow(HWND hwnd)
{
    if (!hwnd)
        return false;

    wchar_t className[256]{};

    HWND root = GetAncestor(hwnd, GA_ROOTOWNER);
    if (root)
    {
        GetClassNameW(root, className, 256);

        if (
            wcscmp(className, L"Shell_TrayWnd") == 0 ||
            wcscmp(className, L"Shell_SecondaryTrayWnd") == 0
        )
        {
            return true;
        }
    }

    HWND current = hwnd;

    while (current)
    {
        ZeroMemory(className, sizeof(className));
        GetClassNameW(current, className, 256);

        if (
            wcscmp(className, L"Shell_TrayWnd") == 0 ||
            wcscmp(className, L"Shell_SecondaryTrayWnd") == 0
        )
        {
            return true;
        }

        current = GetParent(current);
    }

    return false;
}

static bool IsCursorOverTaskbar()
{
    POINT pt{};
    return GetCursorPos(&pt) && IsTaskbarWindow(WindowFromPoint(pt));
}

static bool QueueTaskbarWheel(short delta)
{
    const int accumulated = g_wheelRemainder + delta;
    const int steps = accumulated / WHEEL_DELTA;
    if (steps && !PostMessageW(g_mainWindow, WM_CHANGE_VOLUME,
                              static_cast<WPARAM>(steps), 0))
        return false;
    g_wheelRemainder = accumulated % WHEEL_DELTA;
    return true;
}

static LRESULT CALLBACK MouseHookProc(
    int nCode,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (
        nCode == HC_ACTION &&
        wParam == WM_MOUSEWHEEL
    )
    {
        auto* info =
            reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        if (!IsCursorOverTaskbar())
        {
            g_wheelRemainder = 0;
            return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
        }

        short delta =
            GET_WHEEL_DELTA_WPARAM(info->mouseData);

        if (!QueueTaskbarWheel(delta))
            return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);

        return 1;
    }

    return CallNextHookEx(
        g_mouseHook,
        nCode,
        wParam,
        lParam
    );
}

static LRESULT CALLBACK MainWindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (g_taskbarCreated && msg == g_taskbarCreated)
    {
        AddTrayIcon(hwnd);
        return 0;
    }
    switch (msg)
    {
    case WM_CHANGE_VOLUME:
        ChangeVolume(static_cast<int>(static_cast<INT_PTR>(wParam)));
        return 0;

    case WM_AUDIO_DEVICE_CHANGED:
        RefreshAudioDevice();
        return 0;

    case WM_TRAYICON:
    {
        UINT event = LOWORD(lParam);

        if (
            event == WM_LBUTTONDBLCLK ||
            event == NIN_SELECT ||
            event == NIN_KEYSELECT
        )
        {
            ShowSettingsWindow(hwnd);
            return 0;
        }

        if (
            event == WM_RBUTTONUP ||
            event == WM_CONTEXTMENU
        )
        {
            ShowTrayMenu(hwnd);
            return 0;
        }

        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case CMD_TRAY_SETTINGS:
            ShowSettingsWindow(hwnd);
            return 0;

        case CMD_TRAY_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int
)
{
    g_hInstance = hInstance;

    g_singleInstanceMutex = CreateMutexW(
        nullptr,
        TRUE,
        L"TaskbarVolume.SingleInstance"
    );

    if (
        !g_singleInstanceMutex ||
        GetLastError() == ERROR_ALREADY_EXISTS
    )
    {
        if (g_singleInstanceMutex) CloseHandle(g_singleInstanceMutex);
        return 0;
    }

    SetOverlayEnabled(
        LoadOverlaySetting()
    );

    if (!CreateOverlayWindow(hInstance))
    {
        CloseHandle(g_singleInstanceMutex);
        return 1;
    }

    const wchar_t MAIN_CLASS[] =
        L"TaskbarVolumeMainWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_CLASS;
    wc.hIcon = LoadAppIcon();

    if (!RegisterClassW(&wc))
    {
        if (wc.hIcon) DestroyIcon(wc.hIcon);
        DestroyOverlayWindow();
        CloseHandle(g_singleInstanceMutex);
        return 1;
    }

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    auto cleanup = [&]()
    {
        if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = nullptr; }
        CleanupAudio();
        if (IsWindow(g_mainWindow)) DestroyWindow(g_mainWindow);
        g_mainWindow = nullptr;
        DestroyOverlayWindow();
        UnregisterClassW(MAIN_CLASS, hInstance);
        if (wc.hIcon) DestroyIcon(wc.hIcon);
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = nullptr;
    };

    g_mainWindow = CreateWindowExW(
        0,
        MAIN_CLASS,
        L"Taskbar Volume",
        0,
        0, 0, 0, 0,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_mainWindow)
    {
        cleanup();
        return 1;
    }

    if (!InitAudio() || !AddTrayIcon(g_mainWindow))
    {
        MessageBoxW(nullptr, L"Failed to initialize audio or the tray icon.",
                    L"Taskbar Volume", MB_OK | MB_ICONERROR);
        cleanup();
        return 1;
    }

    g_mouseHook = SetWindowsHookExW(
        WH_MOUSE_LL,
        MouseHookProc,
        hInstance,
        0
    );

    if (!g_mouseHook)
    {
        RemoveTrayIcon(g_mainWindow);

        MessageBoxW(
            nullptr,
            L"Failed to install mouse hook.",
            L"Taskbar Volume",
            MB_OK | MB_ICONERROR
        );

        cleanup();
        return 1;
    }

    MSG msg{};

    int result;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    cleanup();
    return result == -1 ? 1 : 0;
}
