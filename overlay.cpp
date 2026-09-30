#include "overlay.h"

#include <string>

#pragma comment(lib, "Gdi32.lib")

static HWND g_overlayWindow = nullptr;
static std::wstring g_text;
static bool g_enabled = true;
static HFONT g_font = nullptr;
static HBRUSH g_background = nullptr;

static constexpr UINT_PTR OVERLAY_TIMER_ID = 1;
static constexpr int OVERLAY_DURATION_MS = 900;

static LRESULT CALLBACK OverlayProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (msg)
    {
    case WM_CREATE:
        g_background = CreateSolidBrush(RGB(32, 32, 32));
        g_font = CreateFontW(38, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!g_background || !g_font) return -1;
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, OVERLAY_TIMER_ID);
        if (g_font) DeleteObject(g_font);
        if (g_background) DeleteObject(g_background);
        g_font = nullptr;
        g_background = nullptr;
        g_overlayWindow = nullptr;
        return 0;

    case WM_TIMER:
        if (wParam == OVERLAY_TIMER_ID)
        {
            KillTimer(hwnd, OVERLAY_TIMER_ID);
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);

        FillRect(hdc, &rc, g_background);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        HFONT oldFont = reinterpret_cast<HFONT>(
            SelectObject(hdc, g_font)
        );

        DrawTextW(
            hdc,
            g_text.c_str(),
            -1,
            &rc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE
        );

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool CreateOverlayWindow(HINSTANCE hInstance)
{
    const wchar_t CLASS_NAME[] = L"TaskbarVolumeOverlayWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));

    if (!RegisterClassW(&wc)) return false;

    g_overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW |
        WS_EX_NOACTIVATE |
        WS_EX_LAYERED |
        WS_EX_TRANSPARENT,
        CLASS_NAME,
        L"",
        WS_POPUP,
        0, 0, 82, 62,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_overlayWindow)
        return false;

    SetLayeredWindowAttributes(
        g_overlayWindow,
        0,
        225,
        LWA_ALPHA
    );

    return true;
}

void ShowVolumeOverlay(int volume)
{
    if (!g_enabled || !g_overlayWindow)
        return;

    g_text = std::to_wstring(volume);

    POINT cursor{};
    GetCursorPos(&cursor);

    HMONITOR monitor = MonitorFromPoint(
        cursor,
        MONITOR_DEFAULTTOPRIMARY
    );

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(monitor, &mi))
        return;

    constexpr int width = 82;
    constexpr int height = 62;
    constexpr int rightMargin = 20;
    constexpr int bottomMargin = 20;

    int x = mi.rcWork.right - width - rightMargin;
    int y = mi.rcWork.bottom - height - bottomMargin;

    SetWindowPos(
        g_overlayWindow,
        HWND_TOPMOST,
        x, y,
        width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );

    InvalidateRect(g_overlayWindow, nullptr, TRUE);

    if (!SetTimer(
        g_overlayWindow,
        OVERLAY_TIMER_ID,
        OVERLAY_DURATION_MS,
        nullptr
    )) ShowWindow(g_overlayWindow, SW_HIDE);
}

void SetOverlayEnabled(bool enabled)
{
    g_enabled = enabled;
    if (!enabled && g_overlayWindow)
    {
        KillTimer(g_overlayWindow, OVERLAY_TIMER_ID);
        ShowWindow(g_overlayWindow, SW_HIDE);
    }
}

void DestroyOverlayWindow()
{
    if (g_overlayWindow) DestroyWindow(g_overlayWindow);
}

bool GetOverlayEnabled()
{
    return g_enabled;
}
