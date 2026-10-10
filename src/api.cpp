#include "pch.h"
#include "internal.h"

namespace VeloHook {

using Internal::GLBackend;
using Internal::Graphics;
using Internal::InlineHook;
using Internal::IsInitialized;
using Internal::VKBackend;
using Internal::VTHook;

void** GetSwapChainVTable() {
    return IsInitialized() ? Graphics::Get().SwapChainVT() : nullptr;
}

void** GetDeviceVTable() {
    return IsInitialized() ? Graphics::Get().DeviceVT() : nullptr;
}

void** GetContextVTable() {
    return IsInitialized() ? Graphics::Get().ContextVT() : nullptr;
}

void** GetCommandQueueVTable() {
    return IsInitialized() ? Graphics::Get().CommandQueueVT() : nullptr;
}

static Status HookVT(void** vt, uint32_t index, void* detour, void** original) {
    if (!IsInitialized()) return Status::NotInitialized;
    if (!vt) return Status::VTableNotFound;
    return VTHook::Get().Hook(vt, index, detour, original);
}

static Status UnhookVT(void** vt, uint32_t index) {
    if (!IsInitialized()) return Status::NotInitialized;
    if (!vt) return Status::VTableNotFound;
    return VTHook::Get().Unhook(vt, index);
}

Status HookSwapChain(uint32_t index, void* detour, void** original) {
    return HookVT(GetSwapChainVTable(), index, detour, original);
}

Status HookDevice(uint32_t index, void* detour, void** original) {
    return HookVT(GetDeviceVTable(), index, detour, original);
}

Status HookContext(uint32_t index, void* detour, void** original) {
    return HookVT(GetContextVTable(), index, detour, original);
}

Status HookCommandQueue(uint32_t index, void* detour, void** original) {
    return HookVT(GetCommandQueueVTable(), index, detour, original);
}

Status UnhookSwapChain(uint32_t index) {
    return UnhookVT(GetSwapChainVTable(), index);
}

Status UnhookDevice(uint32_t index) {
    return UnhookVT(GetDeviceVTable(), index);
}

Status UnhookContext(uint32_t index) {
    return UnhookVT(GetContextVTable(), index);
}

Status UnhookCommandQueue(uint32_t index) {
    return UnhookVT(GetCommandQueueVTable(), index);
}

void* GetOpenGLFunction(OpenGL::Function fn) {
    return GLBackend::Get().Function(fn);
}

void* GetVulkanFunction(Vulkan::Function fn) {
    return VKBackend::Get().Function(fn);
}

static Status HookResolved(void* target, void* detour, void** original) {
    if (!target) return Status::APIUnavailable;
    return InlineHook::Get().Hook(target, detour, original);
}

static Status UnhookResolved(void* target) {
    if (!target) return Status::APIUnavailable;
    return InlineHook::Get().Unhook(target);
}

Status HookOpenGL(OpenGL::Function fn, void* detour, void** original) {
    return HookResolved(GetOpenGLFunction(fn), detour, original);
}

Status HookVulkan(Vulkan::Function fn, void* detour, void** original) {
    return HookResolved(GetVulkanFunction(fn), detour, original);
}

Status UnhookOpenGL(OpenGL::Function fn) {
    return UnhookResolved(GetOpenGLFunction(fn));
}

Status UnhookVulkan(Vulkan::Function fn) {
    return UnhookResolved(GetVulkanFunction(fn));
}

Status HookFunction(void* target, void* detour, void** original) {
    return InlineHook::Get().Hook(target, detour, original);
}

Status HookExport(const char* moduleName, const char* exportName, void* detour, void** original) {
    if (!moduleName || !exportName) return Status::InvalidParameter;

    HMODULE mod = GetModuleHandleA(moduleName);
    if (!mod) return Status::FunctionNotFound;

    void* target = reinterpret_cast<void*>(GetProcAddress(mod, exportName));
    if (!target) return Status::FunctionNotFound;

    return InlineHook::Get().Hook(target, detour, original);
}

Status UnhookFunction(void* target) {
    return InlineHook::Get().Unhook(target);
}

HWND GetWindow() {
    if (!IsInitialized()) return nullptr;

    HWND game = Internal::GameWindow();
    return game ? game : Graphics::Get().Window();
}

void* GetSwapChain() {
    return IsInitialized() ? Graphics::Get().SwapChain() : nullptr;
}

void* GetDevice() {
    return IsInitialized() ? Graphics::Get().Device() : nullptr;
}

void* GetContext() {
    return IsInitialized() ? Graphics::Get().Context() : nullptr;
}

void* GetCommandQueue() {
    return IsInitialized() ? Graphics::Get().CommandQueue() : nullptr;
}

GraphicsAPI QuerySwapChainAPI(void* swapChain) {
    if (!swapChain) return GraphicsAPI::None;

    auto* sc = static_cast<IDXGISwapChain*>(swapChain);

    Internal::ComPtr<ID3D12Device> dev12;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&dev12)))) return GraphicsAPI::DirectX12;

    Internal::ComPtr<ID3D11Device> dev11;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&dev11)))) return GraphicsAPI::DirectX11;

    return GraphicsAPI::None;
}

void* GetDeviceFromSwapChain(void* swapChain) {
    if (!swapChain) return nullptr;

    auto* sc = static_cast<IDXGISwapChain*>(swapChain);

    ID3D12Device* dev12 = nullptr;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&dev12)))) return dev12;

    ID3D11Device* dev11 = nullptr;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&dev11)))) return dev11;

    return nullptr;
}

void* GetContextFromDevice(void* device) {
    if (!device) return nullptr;

    Internal::ComPtr<ID3D11Device> dev11;
    if (FAILED(static_cast<IUnknown*>(device)->QueryInterface(IID_PPV_ARGS(&dev11)))) return nullptr;

    ID3D11DeviceContext* ctx = nullptr;
    dev11->GetImmediateContext(&ctx);
    return ctx;
}

void* GetBackBuffer(void* swapChain, uint32_t index) {
    if (!swapChain) return nullptr;

    auto* sc = static_cast<IDXGISwapChain*>(swapChain);

    ID3D11Texture2D* tex11 = nullptr;
    if (SUCCEEDED(sc->GetBuffer(index, IID_PPV_ARGS(&tex11)))) return tex11;

    ID3D12Resource* tex12 = nullptr;
    if (SUCCEEDED(sc->GetBuffer(index, IID_PPV_ARGS(&tex12)))) return tex12;

    return nullptr;
}

void* CreateRenderTargetView(void* device, void* backBuffer) {
    if (!device || !backBuffer) return nullptr;

    Internal::ComPtr<ID3D11Device> dev11;
    if (FAILED(static_cast<IUnknown*>(device)->QueryInterface(IID_PPV_ARGS(&dev11)))) return nullptr;

    Internal::ComPtr<ID3D11Resource> res;
    if (FAILED(static_cast<IUnknown*>(backBuffer)->QueryInterface(IID_PPV_ARGS(&res)))) return nullptr;

    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(dev11->CreateRenderTargetView(res.Get(), nullptr, &rtv))) return nullptr;
    return rtv;
}

void ReleaseRenderTargetView(void* rtv) {
    ReleaseObject(rtv);
}

void ReleaseObject(void* object) {
    if (object) static_cast<IUnknown*>(object)->Release();
}

bool GetSwapChainSize(void* swapChain, uint32_t* width, uint32_t* height) {
    if (!swapChain || !width || !height) return false;

    DXGI_SWAP_CHAIN_DESC desc;
    if (FAILED(static_cast<IDXGISwapChain*>(swapChain)->GetDesc(&desc))) return false;

    *width = desc.BufferDesc.Width;
    *height = desc.BufferDesc.Height;
    return true;
}

HWND GetSwapChainWindow(void* swapChain) {
    if (!swapChain) return nullptr;

    DXGI_SWAP_CHAIN_DESC desc;
    if (FAILED(static_cast<IDXGISwapChain*>(swapChain)->GetDesc(&desc))) return nullptr;

    return desc.OutputWindow;
}

void* GetOriginalFunction(void** vtable, uint32_t index) {
    return vtable ? VTHook::Get().Original(vtable, index) : nullptr;
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
        case Status::FunctionNotFound: return "Function not found";
        case Status::UnsupportedFunction: return "Unsupported function";
        case Status::MemoryAllocationFailed: return "Memory allocation failed";
        case Status::APIUnavailable: return "Graphics API unavailable";
        case Status::HooksBusy: return "Hooks still executing";
        default: return "Unknown";
    }
}

}
