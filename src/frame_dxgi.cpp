#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

namespace {

using Present_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeBuffers_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ResizeBuffers1_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using ExecuteCommandLists_t = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

std::mutex g_queueMtx;
ComPtr<ID3D12CommandQueue> g_queue;
std::atomic<IDXGISwapChain*> g_queueOwner{};
ExecuteCommandLists_t g_executeCommandLists{};

void Capture(ID3D12CommandQueue* queue, IDXGISwapChain* owner) {
    std::lock_guard lk(g_queueMtx);
    g_queue = queue;
    g_queueOwner.store(owner, std::memory_order_release);
}

ID3D12CommandQueue* ResolveQueue(IDXGISwapChain* sc, ID3D12Device* device) {
    ComPtr<ID3D12CommandQueue> direct;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&direct)))) {
        if (g_queueOwner.load(std::memory_order_acquire) != sc) Capture(direct.Get(), sc);
        return direct.Get();
    }

    std::lock_guard lk(g_queueMtx);
    if (!g_queue) return nullptr;
    if (g_queueOwner.load(std::memory_order_relaxed) == sc) return g_queue.Get();

    ComPtr<ID3D12Device> owner;
    if (SUCCEEDED(g_queue->GetDevice(IID_PPV_ARGS(&owner))) && owner.Get() == device) {
        g_queueOwner.store(sc, std::memory_order_release);
        return g_queue.Get();
    }

    g_queue.Reset();
    g_queueOwner.store(nullptr, std::memory_order_release);
    return nullptr;
}

void STDMETHODCALLTYPE ExecuteCommandLists(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    HookScope scope;

    if (!g_queueOwner.load(std::memory_order_acquire) && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        std::lock_guard lk(g_queueMtx);
        if (!g_queueOwner.load(std::memory_order_relaxed)) g_queue = queue;
    }

    g_executeCommandLists(queue, count, lists);
}

bool Describe(IDXGISwapChain* sc, FrameContext& ctx) {
    DXGI_SWAP_CHAIN_DESC desc;
    if (FAILED(sc->GetDesc(&desc))) return false;

    ctx.window = desc.OutputWindow;
    ctx.width = desc.BufferDesc.Width;
    ctx.height = desc.BufferDesc.Height;
    ctx.bufferCount = desc.BufferCount;
    ctx.swapChain = sc;

    ComPtr<ID3D12Device> device12;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&device12)))) {
        ctx.api = GraphicsAPI::DirectX12;
        ctx.device = device12.Get();
        ctx.commandQueue = ResolveQueue(sc, device12.Get());

        ComPtr<IDXGISwapChain3> sc3;
        if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc3)))) ctx.bufferIndex = sc3->GetCurrentBackBufferIndex();

        return ctx.commandQueue != nullptr;
    }

    ComPtr<ID3D11Device> device11;
    if (FAILED(sc->GetDevice(IID_PPV_ARGS(&device11)))) return false;

    ComPtr<ID3D11DeviceContext> context;
    device11->GetImmediateContext(&context);

    ctx.api = GraphicsAPI::DirectX11;
    ctx.device = device11.Get();
    ctx.context = context.Get();
    return true;
}

uint64_t Target(IDXGISwapChain* sc) {
    return reinterpret_cast<uintptr_t>(sc);
}

void OnFrame(IDXGISwapChain* sc, UINT flags) {
    if (flags & DXGI_PRESENT_TEST) return;

    FrameContext ctx{};
    if (Describe(sc, ctx)) Frame::Present(ctx, Target(sc));
}

void BeginResize(IDXGISwapChain* sc) {
    FrameContext ctx{};
    if (Describe(sc, ctx)) Frame::ResizeBegin(ctx, Target(sc));
}

void EndResize(IDXGISwapChain* sc) {
    FrameContext ctx{};
    if (Describe(sc, ctx)) Frame::ResizeEnd(ctx, Target(sc));
}

Status Layer(Status st) {
    return st == Status::HookAlreadyExists ? Status::Success : st;
}

template <int N>
struct Detours {
    static inline Present_t present{};
    static inline Present1_t present1{};
    static inline ResizeBuffers_t resizeBuffers{};
    static inline ResizeBuffers1_t resizeBuffers1{};

    static HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* sc, UINT sync, UINT flags) {
        HookScope scope;
        OnFrame(sc, flags);
        return present(sc, sync, flags);
    }

    static HRESULT STDMETHODCALLTYPE Present1(IDXGISwapChain1* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* params) {
        HookScope scope;
        OnFrame(sc, flags);
        return present1(sc, sync, flags, params);
    }

    static HRESULT STDMETHODCALLTYPE ResizeBuffers(IDXGISwapChain* sc, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
        HookScope scope;
        BeginResize(sc);
        HRESULT hr = resizeBuffers(sc, count, width, height, format, flags);
        EndResize(sc);
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE ResizeBuffers1(IDXGISwapChain3* sc, UINT count, UINT width, UINT height, DXGI_FORMAT format,
                                                    UINT flags, const UINT* nodeMasks, IUnknown* const* queues) {
        HookScope scope;
        BeginResize(sc);
        HRESULT hr = resizeBuffers1(sc, count, width, height, format, flags, nodeMasks, queues);

        ComPtr<ID3D12CommandQueue> queue;
        if (SUCCEEDED(hr) && queues && queues[0] && SUCCEEDED(queues[0]->QueryInterface(IID_PPV_ARGS(&queue)))) {
            Capture(queue.Get(), sc);
        }

        EndResize(sc);
        return hr;
    }

    static Status Install(Layers& layers, IDXGISwapChain* sc) {
        Status st = Layer(layers.Add(VTableOf(sc), SwapChain::Present, &Present, &present));
        if (st != Status::Success) return st;

        st = Layer(layers.Add(VTableOf(sc), SwapChain::ResizeBuffers, &ResizeBuffers, &resizeBuffers));
        if (st != Status::Success) return st;

        ComPtr<IDXGISwapChain1> sc1;
        if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc1)))) {
            st = Layer(layers.Add(VTableOf(sc1.Get()), SwapChain1::Present1, &Present1, &present1));
            if (st != Status::Success) return st;
        }

        ComPtr<IDXGISwapChain3> sc3;
        if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc3)))) {
            st = Layer(layers.Add(VTableOf(sc3.Get()), SwapChain3::ResizeBuffers1, &ResizeBuffers1, &resizeBuffers1));
            if (st != Status::Success) return st;
        }

        return Status::Success;
    }

    static void Reset() {
        present = nullptr;
        present1 = nullptr;
        resizeBuffers = nullptr;
        resizeBuffers1 = nullptr;
    }
};

}

Status InstallDXGIFrame(Layers& layers) {
    Graphics& gfx = Graphics::Get();
    IDXGISwapChain* sc11 = gfx.DX11SwapChain();
    IDXGISwapChain* sc12 = gfx.DX12SwapChain();

    if (!sc11 && !sc12) return Status::APIUnavailable;

    if (sc11) {
        Status st = Detours<0>::Install(layers, sc11);
        if (st != Status::Success) return st;
    }

    if (sc12 && (!sc11 || VTableOf(sc11) != VTableOf(sc12))) {
        Status st = Detours<1>::Install(layers, sc12);
        if (st != Status::Success) return st;
    }

    if (ID3D12CommandQueue* queue = gfx.DX12Queue()) {
        Status st = layers.Add(VTableOf(queue), CommandQueue12::ExecuteCommandLists, &ExecuteCommandLists, &g_executeCommandLists);
        if (st != Status::Success) return st;
    }

    return Status::Success;
}

void ResetDXGIFrame() {
    Detours<0>::Reset();
    Detours<1>::Reset();
    g_executeCommandLists = nullptr;
    Capture(nullptr, nullptr);
}

}}
