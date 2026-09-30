#include <windows.h>
#include <cstdio>

static bool Launch(PROCESS_INFORMATION& process)
{
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    wchar_t path[MAX_PATH]{};
    if (!GetFullPathNameW(L"TaskbarVolume.exe", MAX_PATH, path, nullptr)) return false;
    return CreateProcessW(path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process) != FALSE;
}
static void CloseHandles(PROCESS_INFORMATION& process)
{
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
}
int main()
{
    HWND old = FindWindowExW(HWND_MESSAGE, nullptr, L"TaskbarVolumeMainWindow", nullptr);
    if (!old) old = FindWindowW(L"TaskbarVolumeMainWindow", nullptr);
    if (old) {
        DWORD pid = 0; GetWindowThreadProcessId(old, &pid);
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        DWORD_PTR result = 0;
        if (!process || !SendMessageTimeoutW(old, WM_COMMAND, 1002, 0, SMTO_ABORTIFHUNG, 2000, &result)) return 1;
        DWORD waited = WaitForSingleObject(process, 5000); CloseHandle(process);
        if (waited != WAIT_OBJECT_0) return 1;
        std::puts("PASS: previous version exited gracefully");
    }
    PROCESS_INFORMATION first{};
    if (!Launch(first)) return 1;
    HWND window = nullptr;
    for (int i = 0; i < 100 && !window; ++i) {
        Sleep(50); window = FindWindowW(L"TaskbarVolumeMainWindow", nullptr);
        if (WaitForSingleObject(first.hProcess, 0) == WAIT_OBJECT_0) break;
    }
    if (!window) { CloseHandles(first); return 1; }
    std::puts("PASS: complete application startup");
    PROCESS_INFORMATION second{};
    if (!Launch(second)) return 1;
    DWORD secondExit = 1;
    bool singleton = WaitForSingleObject(second.hProcess, 5000) == WAIT_OBJECT_0 &&
                     GetExitCodeProcess(second.hProcess, &secondExit) && secondExit == 0;
    CloseHandles(second);
    FILETIME created{}, exited{}, kernel1{}, user1{}, kernel2{}, user2{};
    GetProcessTimes(first.hProcess, &created, &exited, &kernel1, &user1);
    Sleep(5000);
    GetProcessTimes(first.hProcess, &created, &exited, &kernel2, &user2);
    ULARGE_INTEGER beforeK{}, beforeU{}, afterK{}, afterU{};
    beforeK.LowPart = kernel1.dwLowDateTime; beforeK.HighPart = kernel1.dwHighDateTime;
    beforeU.LowPart = user1.dwLowDateTime; beforeU.HighPart = user1.dwHighDateTime;
    afterK.LowPart = kernel2.dwLowDateTime; afterK.HighPart = kernel2.dwHighDateTime;
    afterU.LowPart = user2.dwLowDateTime; afterU.HighPart = user2.dwHighDateTime;
    std::printf("Idle CPU time over 5 seconds: %.3f ms\n", static_cast<double>(afterK.QuadPart + afterU.QuadPart - beforeK.QuadPart - beforeU.QuadPart) / 10000);
    DWORD_PTR result = 0;
    bool requestedExit = SendMessageTimeoutW(window, WM_COMMAND, 1002, 0, SMTO_ABORTIFHUNG, 2000, &result) != 0;
    DWORD firstExit = 1;
    bool cleanExit = requestedExit && WaitForSingleObject(first.hProcess, 5000) == WAIT_OBJECT_0 &&
                     GetExitCodeProcess(first.hProcess, &firstExit) && firstExit == 0;
    CloseHandles(first);
    if (!singleton || !cleanExit) return 1;
    std::puts("PASS: second instance exits without disturbing first");
    std::puts("PASS: tray Exit command shuts down cleanly");
    return 0;
}


