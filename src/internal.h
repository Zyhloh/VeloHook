#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <vector>
#include <mutex>
#include <atomic>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace VeloHook {
namespace Internal {

struct VTableHook {
    void** vtable;
    uint32_t index;
    void* original;
    void* detour;
    int32_t refCount;
};

class Graphics {
public:
    static Graphics& Get();
    
    bool Init();
    void Release();
    
    int Detect();
    bool SetupDX11();
    bool SetupDX12();
    
    void** SwapChainVT() const { return m_scVT; }
    void** DeviceVT() const { return m_devVT; }
    void** ContextVT() const { return m_ctxVT; }
    void** CommandQueueVT() const { return m_cmdVT; }
    
    HWND Window() const { return m_hwnd; }
    void* SwapChain() const { return m_sc; }
    void* Device() const { return m_dev; }
    void* Context() const { return m_ctx; }
    void* CommandQueue() const { return m_cmd; }
    
    int ActiveAPI() const { return m_api; }
    void ForceAPI(int api) { m_forced = api; }
    
private:
    Graphics() = default;
    ~Graphics();
    
    bool MakeDummyWindow();
    void KillDummyWindow();
    
    HWND m_hwnd{};
    HWND m_dummy{};
    void* m_sc{};
    void* m_dev{};
    void* m_ctx{};
    void* m_cmd{};
    
    void** m_scVT{};
    void** m_devVT{};
    void** m_ctxVT{};
    void** m_cmdVT{};
    
    int m_api{};
    int m_forced{};
    bool m_ready{};
    
    ID3D11Device* m_11dev{};
    ID3D11DeviceContext* m_11ctx{};
    IDXGISwapChain* m_11sc{};
    
    ID3D12Device* m_12dev{};
    ID3D12CommandQueue* m_12cmd{};
    IDXGISwapChain3* m_12sc{};
};

class VTHook {
public:
    static VTHook& Get();
    
    bool Hook(void** vt, uint32_t idx, void* detour, void** orig);
    bool Unhook(void** vt, uint32_t idx);
    bool UnhookAll();
    bool SafeUnhookAll(uint32_t timeoutMs = 5000);
    void* Original(void** vt, uint32_t idx);
    
    void EnterHook(void** vt, uint32_t idx);
    void LeaveHook(void** vt, uint32_t idx);
    bool WaitForHook(void** vt, uint32_t idx, uint32_t timeoutMs);
    bool IsShuttingDown() const { return m_shuttingDown.load(std::memory_order_acquire); }
    
private:
    VTHook() = default;
    
    std::vector<VTableHook> m_hooks;
    std::mutex m_mtx;
    std::atomic<bool> m_shuttingDown{false};
};

}}
