#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <mutex>
#include <memory>

#include "../include/VeloHook.h"

namespace VeloHook {
namespace Internal {

using Microsoft::WRL::ComPtr;

HWND FindMainWindow();

std::mutex& LifecycleMutex();
bool IsInitialized();
bool IsShuttingDown();

Status InstallFrameHooks();
void ReleaseFrameHooks();
HWND GameWindow();

namespace Frame {
    void Present(FrameContext& ctx, uint64_t target);
    void ResizeBegin(const FrameContext& ctx, uint64_t target);
    void ResizeEnd(const FrameContext& ctx, uint64_t target);
    bool Last(FrameContext& out);
    void Reset();
}

class Layers {
public:
    Status Add(void** vt, uint32_t idx, void* detour, void** next);
    Status Add(void* target, void* detour, void** next);
    void Rollback();
    void Clear();

    template <typename Fn>
    Status Add(void** vt, uint32_t idx, Fn detour, Fn* next) {
        return Add(vt, idx, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(next));
    }

    template <typename Fn>
    Status Add(void* target, Fn detour, Fn* next) {
        return Add(target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(next));
    }

private:
    struct Slot {
        void** vtable;
        uint32_t index;
        void* target;
    };

    std::vector<Slot> m_slots;
};

Status InstallDXGIFrame(Layers& layers);
Status InstallOpenGLFrame(Layers& layers);
Status InstallVulkanFrame(Layers& layers);
void ResetDXGIFrame();
void ResetOpenGLFrame();
void ResetVulkanFrame();

inline void** VTableOf(void* obj) {
    return obj ? *static_cast<void***>(obj) : nullptr;
}

class Graphics {
public:
    static Graphics& Get();

    bool Setup(GraphicsAPI api);
    void Release();

    void** SwapChainVT() const { return VTableOf(SwapChain()); }
    void** DeviceVT() const { return VTableOf(Device()); }
    void** ContextVT() const { return VTableOf(Context()); }
    void** CommandQueueVT() const { return VTableOf(CommandQueue()); }

    HWND Window();
    void* SwapChain() const;
    void* Device() const;
    void* Context() const;
    void* CommandQueue() const;

    GraphicsAPI ActiveAPI() const { return m_api; }

    bool SetupDX11();
    bool SetupDX12();

    IDXGISwapChain* DX11SwapChain() const { return m_11sc.Get(); }
    IDXGISwapChain* DX12SwapChain() const { return m_12sc.Get(); }
    ID3D12CommandQueue* DX12Queue() const { return m_12cmd.Get(); }

private:
    Graphics() = default;

    GraphicsAPI Detect();

    bool MakeDummyWindow();
    void KillDummyWindow();

    HWND m_hwnd{};
    HWND m_dummy{};
    GraphicsAPI m_api{ GraphicsAPI::None };

    ComPtr<ID3D11Device> m_11dev;
    ComPtr<ID3D11DeviceContext> m_11ctx;
    ComPtr<IDXGISwapChain> m_11sc;

    ComPtr<ID3D12Device> m_12dev;
    ComPtr<ID3D12CommandQueue> m_12cmd;
    ComPtr<IDXGISwapChain3> m_12sc;
};

class GLBackend {
public:
    static GLBackend& Get();

    bool Setup(bool load);
    void Release();
    void* Function(OpenGL::Function fn);

private:
    GLBackend() = default;

    std::mutex m_mtx;
    void* m_fns[OpenGL::Count]{};
    bool m_ready{};
};

class VKBackend {
public:
    static VKBackend& Get();

    bool Setup(bool load);
    void Release();
    void* Function(Vulkan::Function fn);
    void* DeviceProc(const char* name);

private:
    VKBackend() = default;

    bool Create(HMODULE mod);
    void Reset();

    using DestroyFn = void(__stdcall*)(void*, const void*);
    using VoidFn = void(__stdcall*)();
    using ProcFn = VoidFn(__stdcall*)(void*, const char*);

    std::mutex m_mtx;
    ProcFn m_gdpa{};
    void* m_instance{};
    void* m_device{};
    DestroyFn m_destroyInstance{};
    DestroyFn m_destroyDevice{};
    void* m_fns[Vulkan::Count]{};
    bool m_ready{};
};

class InlineHook {
public:
    static InlineHook& Get();

    Status Hook(void* target, void* detour, void** orig, bool internal = false);
    Status Unhook(void* target, bool internal = false);
    bool DisableAll();
    bool UnhookAll();
    bool Active();

private:
    struct Entry {
        void* target;
        void* trampoline;
        void* detour;
        void* layer;
        void** layerNext;
    };

    InlineHook() = default;

    Entry* Find(void* target);

    std::vector<Entry> m_hooks;
    std::mutex m_mtx;
    bool m_init{};
};

class VTHook {
public:
    static VTHook& Get();

    Status Hook(void** vt, uint32_t idx, void* detour, void** orig, bool internal = false);
    Status Unhook(void** vt, uint32_t idx, bool internal = false);
    bool UnhookAll();
    void* Original(void** vt, uint32_t idx);

private:
    struct Entry {
        void** vtable;
        uint32_t index;
        void* original;
        void* detour;
        void* layer;
        void** layerNext;
    };

    VTHook() = default;

    Entry* Find(void** vt, uint32_t idx);

    std::vector<Entry> m_hooks;
    std::mutex m_mtx;
};

}}
