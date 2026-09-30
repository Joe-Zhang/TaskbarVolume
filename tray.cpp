#include "tray.h"
#include "app.h"
#include "settings.h"

#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

static constexpr UINT TRAY_ID = 1;

bool AddTrayIcon(HWND hwnd)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ID;

    nid.uFlags =
        NIF_ICON |
        NIF_MESSAGE |
        NIF_TIP;

    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadAppIcon();

    wcscpy_s(nid.szTip, L"Taskbar Volume");

    bool ok = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;

    if (ok)
    {
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }

    if (nid.hIcon)
        DestroyIcon(nid.hIcon);

    return ok;
}

void RemoveTrayIcon(HWND hwnd)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ID;

    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(
        menu,
        MF_STRING,
        CMD_TRAY_SETTINGS,
        L"Settings..."
    );

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(
        menu,
        MF_STRING,
        CMD_TRAY_EXIT,
        L"Exit"
    );

    POINT pt{};
    GetCursorPos(&pt);

    SetForegroundWindow(hwnd);

    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON,
        pt.x,
        pt.y,
        0,
        hwnd,
        nullptr
    );

    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}
