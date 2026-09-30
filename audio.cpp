#include "audio.h"
#include "app.h"
#include "overlay.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <new>

static IMMDeviceEnumerator* g_enumerator = nullptr;
static IAudioEndpointVolume* g_endpointVolume = nullptr;
static bool g_comInitialized = false;

// System callbacks only post messages; audio objects stay on the UI thread.
class DeviceNotifications final : public IMMNotificationClient
{
    std::atomic<ULONG> refs_{1};
public:
    std::atomic<HWND> window{nullptr};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(IMMNotificationClient))
            return E_NOINTERFACE;
        *object = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG refs = --refs_;
        if (!refs) delete this;
        return refs;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override
    {
        if (flow == eRender && role == eMultimedia)
            if (HWND hwnd = window.load())
                PostMessageW(hwnd, WM_AUDIO_DEVICE_CHANGED, 0, 0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
};
static DeviceNotifications* g_notifications = nullptr;
static bool g_notificationsRegistered = false;

void RefreshAudioDevice()
{
    if (g_endpointVolume) { g_endpointVolume->Release(); g_endpointVolume = nullptr; }
    if (!g_enumerator) return;
    IMMDevice* device = nullptr;
    if (SUCCEEDED(g_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)))
    {
        device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(&g_endpointVolume));
        device->Release();
    }
}

bool InitAudio()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return false;
    g_comInitialized = true;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&g_enumerator));
    if (FAILED(hr)) { CleanupAudio(); return false; }
    g_notifications = new (std::nothrow) DeviceNotifications;
    if (!g_notifications) { CleanupAudio(); return false; }
    g_notifications->window = g_mainWindow;
    hr = g_enumerator->RegisterEndpointNotificationCallback(g_notifications);
    if (FAILED(hr)) { CleanupAudio(); return false; }
    g_notificationsRegistered = true;
    RefreshAudioDevice();
    // Remain available when no output device is connected at startup.
    return true;
}

void CleanupAudio()
{
    if (g_notifications)
    {
        g_notifications->window = nullptr;
        if (g_notificationsRegistered)
            g_enumerator->UnregisterEndpointNotificationCallback(g_notifications);
        g_notificationsRegistered = false;
        g_notifications->Release();
        g_notifications = nullptr;
    }
    if (g_endpointVolume) { g_endpointVolume->Release(); g_endpointVolume = nullptr; }
    if (g_enumerator) { g_enumerator->Release(); g_enumerator = nullptr; }
    if (g_comInitialized) { CoUninitialize(); g_comInitialized = false; }
}

void ChangeVolume(int steps)
{
    if (!steps) return;
    if (!g_endpointVolume) RefreshAudioDevice();
    if (!g_endpointVolume) return;
    float scalar = 0.0f;
    if (FAILED(g_endpointVolume->GetMasterVolumeLevelScalar(&scalar)))
    {
        RefreshAudioDevice();
        if (!g_endpointVolume || FAILED(g_endpointVolume->GetMasterVolumeLevelScalar(&scalar)))
            return;
    }
    int current = std::clamp(static_cast<int>(std::round(scalar * 100.0f)), 0, 100);
    int next = std::clamp(current + std::clamp(steps, -100, 100), 0, 100);
    if (FAILED(g_endpointVolume->SetMasterVolumeLevelScalar(next / 100.0f, nullptr))) return;
    if (next == 0 || steps > 0)
        if (FAILED(g_endpointVolume->SetMute(next == 0, nullptr))) return;
    ShowVolumeOverlay(next);
}
