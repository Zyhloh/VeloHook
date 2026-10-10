#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>
#include <Windows.h>

namespace VeloHook {

enum class Status : int {
    Success = 0,
    NotInitialized,
    AlreadyInitialized,
    MemoryProtectionFailed,
    HookAlreadyExists,
    HookNotFound,
    InvalidParameter,
    DeviceCreationFailed,
    VTableNotFound,
    WrongProcess,
    FunctionNotFound,
    UnsupportedFunction,
    MemoryAllocationFailed,
    APIUnavailable,
    HooksBusy,
    Unknown = -1
};

enum class GraphicsAPI : int {
    Auto = 0,
    DirectX11,
    DirectX12,
    OpenGL,
    Vulkan,
    None
};

namespace SwapChain {
    enum Index : uint32_t {
        QueryInterface = 0,
        AddRef = 1,
        Release = 2,
        SetPrivateData = 3,
        SetPrivateDataInterface = 4,
        GetPrivateData = 5,
        GetParent = 6,
        GetDevice = 7,
        Present = 8,
        GetBuffer = 9,
        SetFullscreenState = 10,
        GetFullscreenState = 11,
        GetDesc = 12,
        ResizeBuffers = 13,
        ResizeTarget = 14,
        GetContainingOutput = 15,
        GetFrameStatistics = 16,
        GetLastPresentCount = 17
    };
}

namespace SwapChain1 {
    enum Index : uint32_t {
        GetDesc1 = 18,
        GetFullscreenDesc = 19,
        GetHwnd = 20,
        GetCoreWindow = 21,
        Present1 = 22,
        IsTemporaryMonoSupported = 23,
        GetRestrictToOutput = 24,
        SetBackgroundColor = 25,
        GetBackgroundColor = 26,
        SetRotation = 27,
        GetRotation = 28
    };
}

namespace SwapChain2 {
    enum Index : uint32_t {
        SetSourceSize = 29,
        GetSourceSize = 30,
        SetMaximumFrameLatency = 31,
        GetMaximumFrameLatency = 32,
        GetFrameLatencyWaitableObject = 33,
        SetMatrixTransform = 34,
        GetMatrixTransform = 35
    };
}

namespace SwapChain3 {
    enum Index : uint32_t {
        GetCurrentBackBufferIndex = 36,
        CheckColorSpaceSupport = 37,
        SetColorSpace1 = 38,
        ResizeBuffers1 = 39
    };
}

namespace Device11 {
    enum Index : uint32_t {
        QueryInterface = 0,
        AddRef = 1,
        Release = 2,
        CreateBuffer = 3,
        CreateTexture1D = 4,
        CreateTexture2D = 5,
        CreateTexture3D = 6,
        CreateShaderResourceView = 7,
        CreateUnorderedAccessView = 8,
        CreateRenderTargetView = 9,
        CreateDepthStencilView = 10,
        CreateInputLayout = 11,
        CreateVertexShader = 12,
        CreateGeometryShader = 13,
        CreateGeometryShaderWithStreamOutput = 14,
        CreatePixelShader = 15,
        CreateHullShader = 16,
        CreateDomainShader = 17,
        CreateComputeShader = 18,
        CreateClassLinkage = 19,
        CreateBlendState = 20,
        CreateDepthStencilState = 21,
        CreateRasterizerState = 22,
        CreateSamplerState = 23,
        CreateQuery = 24,
        CreatePredicate = 25,
        CreateCounter = 26,
        CreateDeferredContext = 27,
        OpenSharedResource = 28,
        CheckFormatSupport = 29,
        CheckMultisampleQualityLevels = 30,
        CheckCounterInfo = 31,
        CheckCounter = 32,
        CheckFeatureSupport = 33,
        GetPrivateData = 34,
        SetPrivateData = 35,
        SetPrivateDataInterface = 36,
        GetFeatureLevel = 37,
        GetCreationFlags = 38,
        GetDeviceRemovedReason = 39,
        GetImmediateContext = 40,
        SetExceptionMode = 41,
        GetExceptionMode = 42
    };
}

namespace Context11 {
    enum Index : uint32_t {
        QueryInterface = 0,
        AddRef = 1,
        Release = 2,
        GetDevice = 3,
        GetPrivateData = 4,
        SetPrivateData = 5,
        SetPrivateDataInterface = 6,
        VSSetConstantBuffers = 7,
        PSSetShaderResources = 8,
        PSSetShader = 9,
        PSSetSamplers = 10,
        VSSetShader = 11,
        DrawIndexed = 12,
        Draw = 13,
        Map = 14,
        Unmap = 15,
        PSSetConstantBuffers = 16,
        IASetInputLayout = 17,
        IASetVertexBuffers = 18,
        IASetIndexBuffer = 19,
        DrawIndexedInstanced = 20,
        DrawInstanced = 21,
        GSSetConstantBuffers = 22,
        GSSetShader = 23,
        IASetPrimitiveTopology = 24,
        VSSetShaderResources = 25,
        VSSetSamplers = 26,
        Begin = 27,
        End = 28,
        GetData = 29,
        SetPredication = 30,
        GSSetShaderResources = 31,
        GSSetSamplers = 32,
        OMSetRenderTargets = 33,
        OMSetRenderTargetsAndUnorderedAccessViews = 34,
        OMSetBlendState = 35,
        OMSetDepthStencilState = 36,
        SOSetTargets = 37,
        DrawAuto = 38,
        DrawIndexedInstancedIndirect = 39,
        DrawInstancedIndirect = 40,
        Dispatch = 41,
        DispatchIndirect = 42,
        RSSetState = 43,
        RSSetViewports = 44,
        RSSetScissorRects = 45,
        CopySubresourceRegion = 46,
        CopyResource = 47,
        UpdateSubresource = 48,
        CopyStructureCount = 49,
        ClearRenderTargetView = 50,
        ClearUnorderedAccessViewUint = 51,
        ClearUnorderedAccessViewFloat = 52,
        ClearDepthStencilView = 53,
        GenerateMips = 54,
        SetResourceMinLOD = 55,
        GetResourceMinLOD = 56,
        ResolveSubresource = 57,
        ExecuteCommandList = 58,
        HSSetShaderResources = 59,
        HSSetShader = 60,
        HSSetSamplers = 61,
        HSSetConstantBuffers = 62,
        DSSetShaderResources = 63,
        DSSetShader = 64,
        DSSetSamplers = 65,
        DSSetConstantBuffers = 66,
        CSSetShaderResources = 67,
        CSSetUnorderedAccessViews = 68,
        CSSetShader = 69,
        CSSetSamplers = 70,
        CSSetConstantBuffers = 71,
        VSGetConstantBuffers = 72,
        PSGetShaderResources = 73,
        PSGetShader = 74,
        PSGetSamplers = 75,
        VSGetShader = 76,
        PSGetConstantBuffers = 77,
        IAGetInputLayout = 78,
        IAGetVertexBuffers = 79,
        IAGetIndexBuffer = 80,
        GSGetConstantBuffers = 81,
        GSGetShader = 82,
        IAGetPrimitiveTopology = 83,
        VSGetShaderResources = 84,
        VSGetSamplers = 85,
        GetPredication = 86,
        GSGetShaderResources = 87,
        GSGetSamplers = 88,
        OMGetRenderTargets = 89,
        OMGetRenderTargetsAndUnorderedAccessViews = 90,
        OMGetBlendState = 91,
        OMGetDepthStencilState = 92,
        SOGetTargets = 93,
        RSGetState = 94,
        RSGetViewports = 95,
        RSGetScissorRects = 96,
        HSGetShaderResources = 97,
        HSGetShader = 98,
        HSGetSamplers = 99,
        HSGetConstantBuffers = 100,
        DSGetShaderResources = 101,
        DSGetShader = 102,
        DSGetSamplers = 103,
        DSGetConstantBuffers = 104,
        CSGetShaderResources = 105,
        CSGetUnorderedAccessViews = 106,
        CSGetShader = 107,
        CSGetSamplers = 108,
        CSGetConstantBuffers = 109,
        ClearState = 110,
        Flush = 111,
        GetType = 112,
        GetContextFlags = 113,
        FinishCommandList = 114
    };
}

namespace CommandQueue12 {
    enum Index : uint32_t {
        QueryInterface = 0,
        AddRef = 1,
        Release = 2,
        GetPrivateData = 3,
        SetPrivateData = 4,
        SetPrivateDataInterface = 5,
        SetName = 6,
        GetDevice = 7,
        UpdateTileMappings = 8,
        CopyTileMappings = 9,
        ExecuteCommandLists = 10,
        SetMarker = 11,
        BeginEvent = 12,
        EndEvent = 13,
        Signal = 14,
        Wait = 15,
        GetTimestampFrequency = 16,
        GetClockCalibration = 17,
        GetDesc = 18
    };
}

namespace OpenGL {
    enum Function : uint32_t {
        SwapBuffers = 0,
        SwapLayerBuffers,
        MakeCurrent,
        CreateContext,
        DeleteContext,
        Count
    };
}

namespace Vulkan {
    enum Function : uint32_t {
        QueuePresentKHR = 0,
        AcquireNextImageKHR,
        CreateSwapchainKHR,
        DestroySwapchainKHR,
        QueueSubmit,
        GetDeviceQueue,
        Count
    };
}

enum class ResizeStage : int {
    Before = 0,
    After
};

struct FrameContext {
    GraphicsAPI api;
    HWND window;
    uint32_t width;
    uint32_t height;
    uint32_t bufferIndex;
    uint32_t bufferCount;
    void* swapChain;
    void* device;
    void* context;
    void* commandQueue;
    HDC hdc;
    uint64_t vkSwapchain;
    int32_t vkFormat;
    void* vkPresentInfo;
};

using PresentCallback = std::function<void(FrameContext&)>;
using ResizeCallback = std::function<void(ResizeStage, const FrameContext&)>;
using WndProcCallback = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

void SetTargetProcess(const char* processName);
void SetTargetProcessW(const wchar_t* processName);
bool IsTargetProcess();

bool IsEngineReady();
bool WaitForEngine(uint32_t timeoutMs = 30000, uint32_t checkIntervalMs = 100);

bool OpenConsole();
void CloseConsole();
void ConsolePrint(const char* fmt, ...);

Status Initialize(GraphicsAPI api = GraphicsAPI::Auto);
Status Shutdown(uint32_t timeoutMs = 3000);
void Unload(HMODULE module = nullptr);
GraphicsAPI GetActiveAPI();

void EnterHook();
void LeaveHook();

class HookScope {
public:
    HookScope() { EnterHook(); }
    ~HookScope() { LeaveHook(); }
    HookScope(const HookScope&) = delete;
    HookScope& operator=(const HookScope&) = delete;
};

Status OnPresent(PresentCallback callback);
Status OnResize(ResizeCallback callback);

Status HookWndProc(WndProcCallback callback, HWND window = nullptr);
Status UnhookWndProc();
void SetInputBlocked(bool blocked);
bool IsInputBlocked();

void** GetSwapChainVTable();
void** GetDeviceVTable();
void** GetContextVTable();
void** GetCommandQueueVTable();

Status HookSwapChain(uint32_t index, void* detour, void** original);
Status HookDevice(uint32_t index, void* detour, void** original);
Status HookContext(uint32_t index, void* detour, void** original);
Status HookCommandQueue(uint32_t index, void* detour, void** original);

Status UnhookSwapChain(uint32_t index);
Status UnhookDevice(uint32_t index);
Status UnhookContext(uint32_t index);
Status UnhookCommandQueue(uint32_t index);

void* GetOpenGLFunction(OpenGL::Function fn);
void* GetVulkanFunction(Vulkan::Function fn);

Status HookOpenGL(OpenGL::Function fn, void* detour, void** original);
Status HookVulkan(Vulkan::Function fn, void* detour, void** original);

Status UnhookOpenGL(OpenGL::Function fn);
Status UnhookVulkan(Vulkan::Function fn);

Status HookFunction(void* target, void* detour, void** original);
Status HookExport(const char* moduleName, const char* exportName, void* detour, void** original);
Status UnhookFunction(void* target);

bool GetLastFrame(FrameContext* frame);
HWND GetGameWindow();
void* GetGameSwapChain();
void* GetGameDevice();
void* GetGameContext();
void* GetGameCommandQueue();

HWND GetWindow();
void* GetSwapChain();
void* GetDevice();
void* GetContext();
void* GetCommandQueue();

GraphicsAPI QuerySwapChainAPI(void* swapChain);

void* GetDeviceFromSwapChain(void* swapChain);
void* GetContextFromDevice(void* device);
void* GetBackBuffer(void* swapChain, uint32_t index = 0);
void* CreateRenderTargetView(void* device, void* backBuffer);
void ReleaseRenderTargetView(void* rtv);
void ReleaseObject(void* object);
bool GetSwapChainSize(void* swapChain, uint32_t* width, uint32_t* height);
HWND GetSwapChainWindow(void* swapChain);

void* GetOriginalFunction(void** vtable, uint32_t index);

const char* StatusToString(Status status);

namespace Detail {
    template <typename T>
    struct Identity { using Type = T; };

    template <typename Fn>
    using EnableIfFunction = std::enable_if_t<std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>, int>;

    template <typename Fn>
    void* Erase(Fn fn) { return reinterpret_cast<void*>(fn); }
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookSwapChain(uint32_t index, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookSwapChain(index, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookDevice(uint32_t index, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookDevice(index, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookContext(uint32_t index, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookContext(index, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookCommandQueue(uint32_t index, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookCommandQueue(index, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookOpenGL(OpenGL::Function fn, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookOpenGL(fn, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookVulkan(Vulkan::Function fn, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookVulkan(fn, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookFunction(Fn target, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookFunction(Detail::Erase(target), Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookFunction(void* target, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookFunction(target, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status HookExport(const char* moduleName, const char* exportName, Fn detour, typename Detail::Identity<Fn>::Type* original) {
    return HookExport(moduleName, exportName, Detail::Erase(detour), reinterpret_cast<void**>(original));
}

template <typename Fn, Detail::EnableIfFunction<Fn> = 0>
Status UnhookFunction(Fn target) {
    return UnhookFunction(Detail::Erase(target));
}

}
