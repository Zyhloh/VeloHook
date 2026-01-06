#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

Graphics& Graphics::Get() {
    static Graphics inst;
    return inst;
}

Graphics::~Graphics() {
    Release();
}

bool Graphics::MakeDummyWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"VH_Dummy";
    
    RegisterClassExW(&wc);
    
    m_dummy = CreateWindowExW(0, L"VH_Dummy", L"", WS_OVERLAPPEDWINDOW,
                              0, 0, 4, 4, nullptr, nullptr, wc.hInstance, nullptr);
    return m_dummy != nullptr;
}

void Graphics::KillDummyWindow() {
    if (m_dummy) {
        DestroyWindow(m_dummy);
        m_dummy = nullptr;
    }
}

bool Graphics::Init() {
    if (m_ready) return true;
    if (!MakeDummyWindow()) return false;
    m_ready = true;
    return true;
}

void Graphics::Release() {
    if (m_11sc) { m_11sc->Release(); m_11sc = nullptr; }
    if (m_11ctx) { m_11ctx->Release(); m_11ctx = nullptr; }
    if (m_11dev) { m_11dev->Release(); m_11dev = nullptr; }
    
    if (m_12sc) { m_12sc->Release(); m_12sc = nullptr; }
    if (m_12cmd) { m_12cmd->Release(); m_12cmd = nullptr; }
    if (m_12dev) { m_12dev->Release(); m_12dev = nullptr; }
    
    KillDummyWindow();
    m_ready = false;
    m_api = 0;
}

bool Graphics::SetupDX11() {
    if (m_11sc) return true;
    if (!m_dummy && !MakeDummyWindow()) return false;
    
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 4;
    sd.BufferDesc.Height = 4;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_dummy;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                                fls, 2, D3D11_SDK_VERSION, &sd,
                                                &m_11sc, &m_11dev, &fl, &m_11ctx);
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                           fls, 2, D3D11_SDK_VERSION, &sd,
                                           &m_11sc, &m_11dev, &fl, &m_11ctx);
    }
    
    if (FAILED(hr)) return false;
    
    m_scVT = *reinterpret_cast<void***>(m_11sc);
    m_devVT = *reinterpret_cast<void***>(m_11dev);
    m_ctxVT = *reinterpret_cast<void***>(m_11ctx);
    m_sc = m_11sc;
    m_dev = m_11dev;
    m_ctx = m_11ctx;
    
    return true;
}

bool Graphics::SetupDX12() {
    if (m_12sc) return true;
    if (!m_dummy && !MakeDummyWindow()) return false;
    
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_12dev)))) {
        return false;
    }
    
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    
    if (FAILED(m_12dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_12cmd)))) {
        m_12dev->Release();
        m_12dev = nullptr;
        return false;
    }
    
    IDXGIFactory4* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        m_12cmd->Release(); m_12cmd = nullptr;
        m_12dev->Release(); m_12dev = nullptr;
        return false;
    }
    
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 4;
    sd.Height = 4;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    
    IDXGISwapChain1* sc1 = nullptr;
    HRESULT hr = factory->CreateSwapChainForHwnd(m_12cmd, m_dummy, &sd, nullptr, nullptr, &sc1);
    factory->Release();
    
    if (FAILED(hr)) {
        m_12cmd->Release(); m_12cmd = nullptr;
        m_12dev->Release(); m_12dev = nullptr;
        return false;
    }
    
    hr = sc1->QueryInterface(IID_PPV_ARGS(&m_12sc));
    sc1->Release();
    
    if (FAILED(hr)) {
        m_12cmd->Release(); m_12cmd = nullptr;
        m_12dev->Release(); m_12dev = nullptr;
        return false;
    }
    
    m_scVT = *reinterpret_cast<void***>(m_12sc);
    m_devVT = *reinterpret_cast<void***>(m_12dev);
    m_cmdVT = *reinterpret_cast<void***>(m_12cmd);
    m_sc = m_12sc;
    m_dev = m_12dev;
    m_cmd = m_12cmd;
    
    return true;
}

struct WndEnum {
    DWORD pid;
    HWND hwnd;
    int score;
};

static BOOL CALLBACK EnumProc(HWND h, LPARAM p) {
    WndEnum* e = reinterpret_cast<WndEnum*>(p);
    
    DWORD pid;
    GetWindowThreadProcessId(h, &pid);
    if (pid != e->pid) return TRUE;
    
    if (!IsWindowVisible(h)) return TRUE;
    if (GetWindow(h, GW_OWNER)) return TRUE;
    
    RECT r;
    if (!GetClientRect(h, &r)) return TRUE;
    
    int w = r.right - r.left;
    int h2 = r.bottom - r.top;
    if (w < 200 || h2 < 200) return TRUE;
    
    wchar_t cls[64];
    GetClassNameW(h, cls, 64);
    if (wcscmp(cls, L"ConsoleWindowClass") == 0) return TRUE;
    if (wcscmp(cls, L"MSCTFIME UI") == 0) return TRUE;
    if (wcscmp(cls, L"IME") == 0) return TRUE;
    if (wcsstr(cls, L"Tooltip") != nullptr) return TRUE;
    
    int score = w * h2;
    if (score > e->score) {
        e->score = score;
        e->hwnd = h;
    }
    
    return TRUE;
}

static HWND FindWindow() {
    WndEnum e{};
    e.pid = GetCurrentProcessId();
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&e));
    return e.hwnd;
}

int Graphics::Detect() {
    if (m_forced) {
        m_api = m_forced;
        if (m_forced == 1) SetupDX11();
        else if (m_forced == 2) SetupDX12();
        return m_api;
    }
    
    m_hwnd = FindWindow();
    
    HMODULE h12 = GetModuleHandleW(L"d3d12.dll");
    HMODULE h11 = GetModuleHandleW(L"d3d11.dll");
    
    if (!h11 && !h12) {
        return 0;
    }
    
    if (h12 && !h11) {
        if (SetupDX12()) {
            m_api = 2;
            return m_api;
        }
        return 0;
    }
    
    if (h11 && !h12) {
        if (SetupDX11()) {
            m_api = 1;
            return m_api;
        }
        return 0;
    }
    
    if (SetupDX11()) {
        m_api = 1;
        return m_api;
    }
    
    if (SetupDX12()) {
        m_api = 2;
        return m_api;
    }
    
    return 0;
}

}}
