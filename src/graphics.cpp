#include "pch.h"
#include "internal.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace VeloHook {
namespace Internal {

static constexpr wchar_t kDummyClass[] = L"VH_Dummy";

using CreateDXGIFactory1_t = HRESULT(WINAPI*)(REFIID, void**);

template <typename T>
static T LoadExport(const wchar_t* module, const char* name) {
    HMODULE mod = LoadLibraryW(module);
    return mod ? reinterpret_cast<T>(GetProcAddress(mod, name)) : nullptr;
}

struct WndEnum {
    DWORD pid;
    HWND hwnd;
    long long area;
};

static BOOL CALLBACK EnumProc(HWND h, LPARAM p) {
    auto* e = reinterpret_cast<WndEnum*>(p);

    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != e->pid) return TRUE;
    if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;

    RECT r;
    if (!GetClientRect(h, &r)) return TRUE;

    long long w = r.right - r.left;
    long long ht = r.bottom - r.top;
    if (w < 100 || ht < 100) return TRUE;

    wchar_t cls[64]{};
    GetClassNameW(h, cls, static_cast<int>(std::size(cls)));
    if (wcscmp(cls, L"ConsoleWindowClass") == 0) return TRUE;
    if (wcscmp(cls, L"MSCTFIME UI") == 0) return TRUE;
    if (wcscmp(cls, L"IME") == 0) return TRUE;
    if (wcscmp(cls, kDummyClass) == 0) return TRUE;
    if (wcsstr(cls, L"Tooltip")) return TRUE;

    if (w * ht > e->area) {
        e->area = w * ht;
        e->hwnd = h;
    }

    return TRUE;
}

HWND FindMainWindow() {
    WndEnum e{ GetCurrentProcessId(), nullptr, 0 };
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&e));
    return e.hwnd;
}

Graphics& Graphics::Get() {
    static Graphics inst;
    return inst;
}

bool Graphics::MakeDummyWindow() {
    if (m_dummy) return true;

    HINSTANCE inst = reinterpret_cast<HINSTANCE>(&__ImageBase);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = inst;
    wc.lpszClassName = kDummyClass;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    m_dummy = CreateWindowExW(0, kDummyClass, L"", WS_OVERLAPPEDWINDOW,
                              0, 0, 8, 8, nullptr, nullptr, inst, nullptr);
    return m_dummy != nullptr;
}

void Graphics::KillDummyWindow() {
    if (m_dummy) {
        DestroyWindow(m_dummy);
        m_dummy = nullptr;
    }
    UnregisterClassW(kDummyClass, reinterpret_cast<HINSTANCE>(&__ImageBase));
}

bool Graphics::Setup(GraphicsAPI api) {
    if (m_api != GraphicsAPI::None) return true;

    m_hwnd = FindMainWindow();

    if (api == GraphicsAPI::Auto) api = Detect();

    bool ok = false;
    switch (api) {
        case GraphicsAPI::DirectX11: ok = SetupDX11(); break;
        case GraphicsAPI::DirectX12: ok = SetupDX12(); break;
        case GraphicsAPI::OpenGL: ok = GLBackend::Get().Setup(true); break;
        case GraphicsAPI::Vulkan: ok = VKBackend::Get().Setup(true); break;
        default: break;
    }

    if (!ok) {
        Release();
        return false;
    }

    m_api = api;
    return true;
}

void Graphics::Release() {
    m_11sc.Reset();
    m_11ctx.Reset();
    m_11dev.Reset();

    m_12sc.Reset();
    m_12cmd.Reset();
    m_12dev.Reset();

    KillDummyWindow();
    m_hwnd = nullptr;
    m_api = GraphicsAPI::None;
}

GraphicsAPI Graphics::Detect() {
    bool has11 = GetModuleHandleW(L"d3d11.dll") != nullptr;
    bool has12 = GetModuleHandleW(L"d3d12.dll") != nullptr;

    if (!has11 && !has12) {
        if (GetModuleHandleW(L"vulkan-1.dll")) return GraphicsAPI::Vulkan;
        if (GetModuleHandleW(L"opengl32.dll")) return GraphicsAPI::OpenGL;
        return GraphicsAPI::None;
    }

    if (has12 && !has11) return GraphicsAPI::DirectX12;
    if (has11 && !has12) return GraphicsAPI::DirectX11;

    if (SetupDX11()) {
        SetupDX12();
        return GraphicsAPI::DirectX11;
    }
    return GraphicsAPI::DirectX12;
}

bool Graphics::SetupDX11() {
    if (m_11sc) return true;
    if (!MakeDummyWindow()) return false;

    auto create = LoadExport<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(L"d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    if (!create) return false;

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 8;
    sd.BufferDesc.Height = 8;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_dummy;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };

    for (D3D_DRIVER_TYPE driver : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
        HRESULT hr = create(nullptr, driver, nullptr, 0, levels, static_cast<UINT>(std::size(levels)),
                            D3D11_SDK_VERSION, &sd, &m_11sc, &m_11dev, nullptr, &m_11ctx);
        if (SUCCEEDED(hr)) return true;
    }

    return false;
}

bool Graphics::SetupDX12() {
    if (m_12sc) return true;
    if (!MakeDummyWindow()) return false;

    auto createDevice = LoadExport<PFN_D3D12_CREATE_DEVICE>(L"d3d12.dll", "D3D12CreateDevice");
    auto createFactory = LoadExport<CreateDXGIFactory1_t>(L"dxgi.dll", "CreateDXGIFactory1");
    if (!createDevice || !createFactory) return false;

    ComPtr<ID3D12Device> dev;
    if (FAILED(createDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) return false;

    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ComPtr<ID3D12CommandQueue> cmd;
    if (FAILED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&cmd)))) return false;

    ComPtr<IDXGIFactory4> factory;
    if (FAILED(createFactory(IID_PPV_ARGS(&factory)))) return false;

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 8;
    sd.Height = 8;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(factory->CreateSwapChainForHwnd(cmd.Get(), m_dummy, &sd, nullptr, nullptr, &sc1))) return false;

    ComPtr<IDXGISwapChain3> sc;
    if (FAILED(sc1.As(&sc))) return false;

    m_12dev = std::move(dev);
    m_12cmd = std::move(cmd);
    m_12sc = std::move(sc);
    return true;
}

HWND Graphics::Window() {
    if (!m_hwnd || !IsWindow(m_hwnd)) m_hwnd = FindMainWindow();
    return m_hwnd;
}

void* Graphics::SwapChain() const {
    switch (m_api) {
        case GraphicsAPI::DirectX11: return m_11sc.Get();
        case GraphicsAPI::DirectX12: return m_12sc.Get();
        default: return nullptr;
    }
}

void* Graphics::Device() const {
    switch (m_api) {
        case GraphicsAPI::DirectX11: return m_11dev.Get();
        case GraphicsAPI::DirectX12: return m_12dev.Get();
        default: return nullptr;
    }
}

void* Graphics::Context() const {
    return m_api != GraphicsAPI::None ? m_11ctx.Get() : nullptr;
}

void* Graphics::CommandQueue() const {
    return m_api != GraphicsAPI::None ? m_12cmd.Get() : nullptr;
}

}}
