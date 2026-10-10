#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

namespace {

std::atomic<std::shared_ptr<const PresentCallback>> g_presentCallback;
std::atomic<std::shared_ptr<const ResizeCallback>> g_resizeCallback;

std::mutex g_frameMtx;
FrameContext g_last{};
uint64_t g_target{};
bool g_hasFrame{};
bool g_pending{};

Layers g_layers;
bool g_installed{};

thread_local bool t_inFrame = false;

void NotifyResize(ResizeStage stage, const FrameContext& ctx) {
    auto callback = g_resizeCallback.load(std::memory_order_acquire);
    if (callback && *callback) (*callback)(stage, ctx);
}

void Store(const FrameContext& ctx, uint64_t target) {
    g_last = ctx;
    g_last.vkPresentInfo = nullptr;
    g_target = target;
    g_hasFrame = true;
}

}

namespace Frame {

void Present(FrameContext& ctx, uint64_t target) {
    if (t_inFrame || IsShuttingDown()) return;
    t_inFrame = true;

    bool before = false;
    bool after = false;
    {
        std::lock_guard lk(g_frameMtx);

        if (g_hasFrame && (g_pending || g_target != target || g_last.width != ctx.width || g_last.height != ctx.height)) {
            before = !g_pending;
            after = true;
        }

        Store(ctx, target);
        g_pending = false;
    }

    if (before) NotifyResize(ResizeStage::Before, ctx);
    if (after) NotifyResize(ResizeStage::After, ctx);

    auto callback = g_presentCallback.load(std::memory_order_acquire);
    if (callback && *callback) (*callback)(ctx);

    t_inFrame = false;
}

void ResizeBegin(const FrameContext& ctx, uint64_t target) {
    if (t_inFrame || IsShuttingDown()) return;
    {
        std::lock_guard lk(g_frameMtx);
        if (!g_hasFrame || g_pending || g_target != target) return;
        g_pending = true;
    }

    t_inFrame = true;
    NotifyResize(ResizeStage::Before, ctx);
    t_inFrame = false;
}

void ResizeEnd(const FrameContext& ctx, uint64_t target) {
    if (t_inFrame || IsShuttingDown()) return;
    {
        std::lock_guard lk(g_frameMtx);
        if (!g_pending || g_target != target) return;
        Store(ctx, target);
        g_pending = false;
    }

    t_inFrame = true;
    NotifyResize(ResizeStage::After, ctx);
    t_inFrame = false;
}

bool Last(FrameContext& out) {
    std::lock_guard lk(g_frameMtx);
    if (!g_hasFrame) return false;
    out = g_last;
    return true;
}

void Reset() {
    g_presentCallback.store(nullptr, std::memory_order_release);
    g_resizeCallback.store(nullptr, std::memory_order_release);

    std::lock_guard lk(g_frameMtx);
    g_last = {};
    g_target = 0;
    g_hasFrame = false;
    g_pending = false;
}

}

Status Layers::Add(void** vt, uint32_t idx, void* detour, void** next) {
    Status st = VTHook::Get().Hook(vt, idx, detour, next, true);
    if (st == Status::Success) m_slots.push_back({ vt, idx, nullptr });
    return st;
}

Status Layers::Add(void* target, void* detour, void** next) {
    Status st = InlineHook::Get().Hook(target, detour, next, true);
    if (st == Status::Success) m_slots.push_back({ nullptr, 0, target });
    return st;
}

void Layers::Rollback() {
    for (auto it = m_slots.rbegin(); it != m_slots.rend(); ++it) {
        if (it->vtable) {
            VTHook::Get().Unhook(it->vtable, it->index, true);
        } else {
            InlineHook::Get().Unhook(it->target, true);
        }
    }
    m_slots.clear();
}

void Layers::Clear() {
    m_slots.clear();
}

Status InstallFrameHooks() {
    if (g_installed) return Status::Success;

    Status st = Status::APIUnavailable;
    switch (Graphics::Get().ActiveAPI()) {
        case GraphicsAPI::DirectX11:
        case GraphicsAPI::DirectX12: st = InstallDXGIFrame(g_layers); break;
        case GraphicsAPI::OpenGL: st = InstallOpenGLFrame(g_layers); break;
        case GraphicsAPI::Vulkan: st = InstallVulkanFrame(g_layers); break;
        default: break;
    }

    if (st != Status::Success) {
        g_layers.Rollback();
        ResetDXGIFrame();
        ResetOpenGLFrame();
        ResetVulkanFrame();
        return st;
    }

    g_installed = true;
    return Status::Success;
}

void ReleaseFrameHooks() {
    g_layers.Clear();
    g_installed = false;

    Frame::Reset();
    ResetDXGIFrame();
    ResetOpenGLFrame();
    ResetVulkanFrame();
}

HWND GameWindow() {
    FrameContext ctx;
    return Frame::Last(ctx) && IsWindow(ctx.window) ? ctx.window : nullptr;
}

}

static bool Accepting() {
    return Internal::IsInitialized() && !Internal::IsShuttingDown();
}

Status OnPresent(PresentCallback callback) {
    if (!Accepting()) return Status::NotInitialized;

    Internal::g_presentCallback.store(callback ? std::make_shared<const PresentCallback>(std::move(callback)) : nullptr,
                                      std::memory_order_release);
    return Status::Success;
}

Status OnResize(ResizeCallback callback) {
    if (!Accepting()) return Status::NotInitialized;

    Internal::g_resizeCallback.store(callback ? std::make_shared<const ResizeCallback>(std::move(callback)) : nullptr,
                                     std::memory_order_release);
    return Status::Success;
}

bool GetLastFrame(FrameContext* frame) {
    return frame && Internal::IsInitialized() && Internal::Frame::Last(*frame);
}

HWND GetGameWindow() {
    return Internal::IsInitialized() ? Internal::GameWindow() : nullptr;
}

void* GetGameSwapChain() {
    FrameContext ctx;
    return GetLastFrame(&ctx) ? ctx.swapChain : nullptr;
}

void* GetGameDevice() {
    FrameContext ctx;
    return GetLastFrame(&ctx) ? ctx.device : nullptr;
}

void* GetGameContext() {
    FrameContext ctx;
    return GetLastFrame(&ctx) ? ctx.context : nullptr;
}

void* GetGameCommandQueue() {
    FrameContext ctx;
    return GetLastFrame(&ctx) ? ctx.commandQueue : nullptr;
}

}
