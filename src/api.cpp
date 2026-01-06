#include "pch.h"
#include "internal.h"
#include "../include/VeloHook.h"

namespace VeloHook {

static bool g_init = false;
static wchar_t g_targetProcess[260] = {0};

void SetTargetProcess(const char* processName) {
    if (!processName) {
        g_targetProcess[0] = 0;
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, processName, -1, g_targetProcess, 260);
}

void SetTargetProcessW(const wchar_t* processName) {
    if (!processName) {
        g_targetProcess[0] = 0;
        return;
    }
    for (int i = 0; i < 259 && processName[i]; ++i) {
        g_targetProcess[i] = processName[i];
        g_targetProcess[i + 1] = 0;
    }
}

bool IsTargetProcess() {
    if (g_targetProcess[0] == 0) return true;
    
    wchar_t currentProcess[260] = {0};
    if (GetModuleFileNameW(nullptr, currentProcess, 259) == 0) return true;
    
    wchar_t* fileName = currentProcess;
    for (wchar_t* p = currentProcess; *p; ++p) {
        if (*p == L'\\' || *p == L'/') fileName = p + 1;
    }
    
    for (int i = 0; i < 260; ++i) {
        wchar_t a = fileName[i];
        wchar_t b = g_targetProcess[i];
        
        if (a >= L'A' && a <= L'Z') a += 32;
        if (b >= L'A' && b <= L'Z') b += 32;
        
        if (a != b) return false;
        if (a == 0) return true;
    }
    
    return false;
}

static bool CheckModuleExport(const wchar_t* moduleName, const char* exportName) {
    HMODULE mod = GetModuleHandleW(moduleName);
    if (!mod) return false;
    return GetProcAddress(mod, exportName) != nullptr;
}

static bool CheckMemoryValid(void* addr) {
    if (!addr) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(addr, &mbi, sizeof(mbi))) return false;
    return (mbi.State == MEM_COMMIT) && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
}

bool IsEngineReady() {
    if (!IsTargetProcess()) return false;
    
    HMODULE mainMod = GetModuleHandleW(nullptr);
    if (!mainMod) return false;
    
    bool hasDX = (GetModuleHandleW(L"d3d11.dll") != nullptr) || (GetModuleHandleW(L"d3d12.dll") != nullptr);
    if (!hasDX) return false;
    
    bool hasDXGI = (GetModuleHandleW(L"dxgi.dll") != nullptr);
    if (!hasDXGI) return false;
    
    HWND gameWnd = nullptr;
    DWORD pid = GetCurrentProcessId();
    
    struct EnumData { DWORD pid; HWND hwnd; } data = { pid, nullptr };
    
    EnumWindows([](HWND h, LPARAM p) -> BOOL {
        auto* d = reinterpret_cast<EnumData*>(p);
        DWORD wndPid;
        GetWindowThreadProcessId(h, &wndPid);
        if (wndPid == d->pid && IsWindowVisible(h) && !GetWindow(h, GW_OWNER)) {
            RECT r;
            if (GetClientRect(h, &r) && (r.right - r.left) > 100 && (r.bottom - r.top) > 100) {
                d->hwnd = h;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));
    
    return data.hwnd != nullptr;
}

bool WaitForEngine(uint32_t timeoutMs, uint32_t checkIntervalMs) {
    if (!IsTargetProcess()) return false;
    
    auto start = GetTickCount64();
    
    while (true) {
        if (IsEngineReady()) return true;
        
        if (GetTickCount64() - start > timeoutMs) return false;
        
        Sleep(checkIntervalMs);
    }
}

static bool g_consoleOpen = false;
static bool g_consoleOwnedByVelo = false;

bool OpenConsole() {
    if (!IsTargetProcess()) return false;
    if (g_consoleOpen) return true;
    
    if (!AllocConsole()) return false;
    
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
    
    g_consoleOpen = true;
    g_consoleOwnedByVelo = true;
    return true;
}

void CloseConsole() {
    if (!g_consoleOpen) return;
    FreeConsole();
    g_consoleOpen = false;
    g_consoleOwnedByVelo = false;
}

void ConsolePrint(const char* fmt, ...) {
    if (!g_consoleOpen) return;
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

Status Initialize() {
    if (g_init) return Status::AlreadyInitialized;
    if (!IsTargetProcess()) return Status::WrongProcess;
    if (!Internal::Graphics::Get().Init()) return Status::DeviceCreationFailed;
    if (!Internal::Graphics::Get().SetupDX11()) {
        if (!Internal::Graphics::Get().SetupDX12()) {
            return Status::DeviceCreationFailed;
        }
    }
    g_init = true;
    return Status::Success;
}


void** GetSwapChainVTable() {
    return g_init ? Internal::Graphics::Get().SwapChainVT() : nullptr;
}

void** GetDeviceVTable() {
    return g_init ? Internal::Graphics::Get().DeviceVT() : nullptr;
}

void** GetContextVTable() {
    return g_init ? Internal::Graphics::Get().ContextVT() : nullptr;
}

void** GetCommandQueueVTable() {
    return g_init ? Internal::Graphics::Get().CommandQueueVT() : nullptr;
}

Status HookSwapChain(uint32_t index, void* detour, void** original) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().SwapChainVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Hook(vt, index, detour, original)) return Status::HookAlreadyExists;
    return Status::Success;
}

Status HookDevice(uint32_t index, void* detour, void** original) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().DeviceVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Hook(vt, index, detour, original)) return Status::HookAlreadyExists;
    return Status::Success;
}

Status HookContext(uint32_t index, void* detour, void** original) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().ContextVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Hook(vt, index, detour, original)) return Status::HookAlreadyExists;
    return Status::Success;
}

Status HookCommandQueue(uint32_t index, void* detour, void** original) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().CommandQueueVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Hook(vt, index, detour, original)) return Status::HookAlreadyExists;
    return Status::Success;
}

Status UnhookSwapChain(uint32_t index) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().SwapChainVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Unhook(vt, index)) return Status::HookNotFound;
    return Status::Success;
}

Status UnhookDevice(uint32_t index) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().DeviceVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Unhook(vt, index)) return Status::HookNotFound;
    return Status::Success;
}

Status UnhookContext(uint32_t index) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().ContextVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Unhook(vt, index)) return Status::HookNotFound;
    return Status::Success;
}

Status UnhookCommandQueue(uint32_t index) {
    if (!g_init) return Status::NotInitialized;
    void** vt = Internal::Graphics::Get().CommandQueueVT();
    if (!vt) return Status::VTableNotFound;
    if (!Internal::VTHook::Get().Unhook(vt, index)) return Status::HookNotFound;
    return Status::Success;
}

HWND GetWindow() {
    return g_init ? Internal::Graphics::Get().Window() : nullptr;
}

void* GetSwapChain() {
    return g_init ? Internal::Graphics::Get().SwapChain() : nullptr;
}

void* GetDevice() {
    return g_init ? Internal::Graphics::Get().Device() : nullptr;
}

void* GetContext() {
    return g_init ? Internal::Graphics::Get().Context() : nullptr;
}

void* GetCommandQueue() {
    return g_init ? Internal::Graphics::Get().CommandQueue() : nullptr;
}

GraphicsAPI QuerySwapChainAPI(void* swapChain) {
    if (!swapChain) return GraphicsAPI::None;
    
    IDXGISwapChain* sc = static_cast<IDXGISwapChain*>(swapChain);
    
    ID3D12Device* dev12 = nullptr;
    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D12Device), (void**)&dev12)) && dev12) {
        dev12->Release();
        return GraphicsAPI::DirectX12;
    }
    
    ID3D11Device* dev11 = nullptr;
    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&dev11)) && dev11) {
        dev11->Release();
        return GraphicsAPI::DirectX11;
    }
    
    return GraphicsAPI::None;
}

void* GetDeviceFromSwapChain(void* swapChain) {
    if (!swapChain) return nullptr;
    
    IDXGISwapChain* sc = static_cast<IDXGISwapChain*>(swapChain);
    
    ID3D12Device* dev12 = nullptr;
    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D12Device), (void**)&dev12)) && dev12) {
        return dev12;
    }
    
    ID3D11Device* dev11 = nullptr;
    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&dev11)) && dev11) {
        return dev11;
    }
    
    return nullptr;
}

void* GetContextFromDevice(void* device) {
    if (!device) return nullptr;
    
    ID3D11Device* dev11 = nullptr;
    if (SUCCEEDED(static_cast<IUnknown*>(device)->QueryInterface(__uuidof(ID3D11Device), (void**)&dev11)) && dev11) {
        ID3D11DeviceContext* ctx = nullptr;
        dev11->GetImmediateContext(&ctx);
        dev11->Release();
        return ctx;
    }
    
    return nullptr;
}

void* GetBackBuffer(void* swapChain, uint32_t index) {
    if (!swapChain) return nullptr;
    
    IDXGISwapChain* sc = static_cast<IDXGISwapChain*>(swapChain);
    
    ID3D11Texture2D* tex11 = nullptr;
    if (SUCCEEDED(sc->GetBuffer(index, __uuidof(ID3D11Texture2D), (void**)&tex11)) && tex11) {
        return tex11;
    }
    
    ID3D12Resource* tex12 = nullptr;
    if (SUCCEEDED(sc->GetBuffer(index, __uuidof(ID3D12Resource), (void**)&tex12)) && tex12) {
        return tex12;
    }
    
    return nullptr;
}

void* CreateRenderTargetView(void* device, void* backBuffer) {
    if (!device || !backBuffer) return nullptr;
    
    ID3D11Device* dev11 = nullptr;
    if (SUCCEEDED(static_cast<IUnknown*>(device)->QueryInterface(__uuidof(ID3D11Device), (void**)&dev11)) && dev11) {
        ID3D11RenderTargetView* rtv = nullptr;
        HRESULT hr = dev11->CreateRenderTargetView(static_cast<ID3D11Resource*>(backBuffer), nullptr, &rtv);
        dev11->Release();
        if (SUCCEEDED(hr)) return rtv;
    }
    
    return nullptr;
}

void ReleaseRenderTargetView(void* rtv) {
    if (rtv) {
        static_cast<IUnknown*>(rtv)->Release();
    }
}

bool GetSwapChainSize(void* swapChain, uint32_t* width, uint32_t* height) {
    if (!swapChain || !width || !height) return false;
    
    IDXGISwapChain* sc = static_cast<IDXGISwapChain*>(swapChain);
    DXGI_SWAP_CHAIN_DESC desc;
    if (SUCCEEDED(sc->GetDesc(&desc))) {
        *width = desc.BufferDesc.Width;
        *height = desc.BufferDesc.Height;
        return true;
    }
    
    return false;
}

HWND GetSwapChainWindow(void* swapChain) {
    if (!swapChain) return nullptr;
    
    IDXGISwapChain* sc = static_cast<IDXGISwapChain*>(swapChain);
    DXGI_SWAP_CHAIN_DESC desc;
    if (SUCCEEDED(sc->GetDesc(&desc))) {
        return desc.OutputWindow;
    }
    
    return nullptr;
}

void* GetOriginalFunction(void** vtable, uint32_t index) {
    if (!vtable) return nullptr;
    return Internal::VTHook::Get().Original(vtable, index);
}

const char* StatusToString(Status status) {
    switch (status) {
        case Status::Success: return "Success";
        case Status::NotInitialized: return "Not initialized";
        case Status::AlreadyInitialized: return "Already initialized";
        case Status::MemoryProtectionFailed: return "Memory protection failed";
        case Status::HookAlreadyExists: return "Hook already exists";
        case Status::HookNotFound: return "Hook not found";
        case Status::InvalidParameter: return "Invalid parameter";
        case Status::DeviceCreationFailed: return "Device creation failed";
        case Status::VTableNotFound: return "VTable not found";
        case Status::WrongProcess: return "Wrong process";
        default: return "Unknown";
    }
}

}
