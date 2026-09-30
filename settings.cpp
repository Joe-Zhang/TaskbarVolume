#include "settings.h"
#include "overlay.h"

#include <windows.h>
#include <string>

#pragma comment(lib, "Advapi32.lib")

static constexpr wchar_t APP_NAME[] = L"TaskbarVolume";
static constexpr wchar_t RUN_KEY[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static constexpr wchar_t SETTINGS_KEY[] =
    L"Software\\TaskbarVolume";

static HWND g_settingsWindow = nullptr;
static HWND g_startupCheck = nullptr;
static HWND g_overlayCheck = nullptr;

static std::wstring GetExePath()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

bool IsStartWithWindowsEnabled()
{
    HKEY key{};
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        RUN_KEY,
        0,
        KEY_READ,
        &key
    ) != ERROR_SUCCESS)
    {
        return false;
    }

    wchar_t value[MAX_PATH * 2]{};
    DWORD size = sizeof(value);
    DWORD type = 0;

    LONG result = RegQueryValueExW(
        key,
        APP_NAME,
        nullptr,
        &type,
        reinterpret_cast<LPBYTE>(value),
        &size
    );

    RegCloseKey(key);

    if (result != ERROR_SUCCESS || type != REG_SZ)
        return false;

    std::wstring expected = L"\"" + GetExePath() + L"\"";
    return expected == value;
}

bool SetStartWithWindows(bool enabled)
{
    HKEY key{};
    DWORD disposition{};

    if (RegCreateKeyExW(
        HKEY_CURRENT_USER,
        RUN_KEY,
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        &disposition
    ) != ERROR_SUCCESS)
    {
        return false;
    }

    LONG result = ERROR_SUCCESS;

    if (enabled)
    {
        std::wstring value = L"\"" + GetExePath() + L"\"";

        result = RegSetValueExW(
            key,
            APP_NAME,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>(
                (value.size() + 1) * sizeof(wchar_t)
            )
        );
    }
    else
    {
        result = RegDeleteValueW(key, APP_NAME);
        if (result == ERROR_FILE_NOT_FOUND)
            result = ERROR_SUCCESS;
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool LoadOverlaySetting()
{
    HKEY key{};
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        SETTINGS_KEY,
        0,
        KEY_READ,
        &key
    ) != ERROR_SUCCESS)
    {
        return true;
    }

    DWORD value = 1;
    DWORD size = sizeof(value);
    DWORD type = 0;

    LONG result = RegQueryValueExW(
        key,
        L"ShowOverlay",
        nullptr,
        &type,
        reinterpret_cast<LPBYTE>(&value),
        &size
    );

    RegCloseKey(key);

    if (result != ERROR_SUCCESS || type != REG_DWORD)
        return true;

    return value != 0;
}

void SaveOverlaySetting(bool enabled)
{
    HKEY key{};
    DWORD disposition{};

    if (RegCreateKeyExW(
        HKEY_CURRENT_USER,
        SETTINGS_KEY,
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        &disposition
    ) != ERROR_SUCCESS)
    {
        return;
    }

    DWORD value = enabled ? 1 : 0;

    RegSetValueExW(
        key,
        L"ShowOverlay",
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value)
    );

    RegCloseKey(key);
}

static LRESULT CALLBACK SettingsProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HFONT font = reinterpret_cast<HFONT>(
            GetStockObject(DEFAULT_GUI_FONT)
        );

        g_startupCheck = CreateWindowExW(
            0,
            L"BUTTON",
            L"Start with Windows",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            22, 24, 220, 24,
            hwnd,
            reinterpret_cast<HMENU>(100),
            nullptr,
            nullptr
        );

        g_overlayCheck = CreateWindowExW(
            0,
            L"BUTTON",
            L"Show volume overlay",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            22, 58, 220, 24,
            hwnd,
            reinterpret_cast<HMENU>(101),
            nullptr,
            nullptr
        );

        SendMessageW(g_startupCheck, WM_SETFONT,
                     reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_overlayCheck, WM_SETFONT,
                     reinterpret_cast<WPARAM>(font), TRUE);

        SendMessageW(
            g_startupCheck,
            BM_SETCHECK,
            IsStartWithWindowsEnabled()
                ? BST_CHECKED
                : BST_UNCHECKED,
            0
        );

        SendMessageW(
            g_overlayCheck,
            BM_SETCHECK,
            GetOverlayEnabled()
                ? BST_CHECKED
                : BST_UNCHECKED,
            0
        );

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 100 &&
            HIWORD(wParam) == BN_CLICKED)
        {
            bool enabled =
                SendMessageW(
                    g_startupCheck,
                    BM_GETCHECK,
                    0,
                    0
                ) == BST_CHECKED;

            if (!SetStartWithWindows(enabled))
            {
                SendMessageW(g_startupCheck, BM_SETCHECK,
                    IsStartWithWindowsEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
                MessageBoxW(
                    hwnd,
                    L"Could not update the Windows startup setting.",
                    L"Taskbar Volume",
                    MB_OK | MB_ICONERROR
                );
            }
            return 0;
        }

        if (LOWORD(wParam) == 101 &&
            HIWORD(wParam) == BN_CLICKED)
        {
            bool enabled =
                SendMessageW(
                    g_overlayCheck,
                    BM_GETCHECK,
                    0,
                    0
                ) == BST_CHECKED;

            SetOverlayEnabled(enabled);
            SaveOverlaySetting(enabled);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_settingsWindow = nullptr;
        g_startupCheck = nullptr;
        g_overlayCheck = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowSettingsWindow(HWND owner)
{
    if (g_settingsWindow)
    {
        ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_settingsWindow);
        return;
    }

    HINSTANCE hInstance =
        reinterpret_cast<HINSTANCE>(
            GetWindowLongPtrW(owner, GWLP_HINSTANCE)
        );

    const wchar_t CLASS_NAME[] =
        L"TaskbarVolumeSettingsWindow";

    static bool registered = false;

    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = SettingsProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursorW(
            nullptr,
            MAKEINTRESOURCEW(32512)
        );
        wc.hbrBackground =
            reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        RegisterClassW(&wc);
        registered = true;
    }

    g_settingsWindow = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"Taskbar Volume Settings",
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        300,
        145,
        owner,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_settingsWindow)
        return;

    ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
    UpdateWindow(g_settingsWindow);
}
