#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

namespace {

using SwapBuffers_t = BOOL(WINAPI*)(HDC);
using GetCurrentContext_t = HGLRC(WINAPI*)();

SwapBuffers_t g_swapBuffers{};
GetCurrentContext_t g_getCurrentContext{};

BOOL WINAPI SwapBuffers(HDC hdc) {
    HookScope scope;

    HWND window = WindowFromDC(hdc);
    RECT rc{};
    if (window && GetClientRect(window, &rc) && rc.right > rc.left && rc.bottom > rc.top) {
        FrameContext ctx{};
        ctx.api = GraphicsAPI::OpenGL;
        ctx.window = window;
        ctx.width = static_cast<uint32_t>(rc.right - rc.left);
        ctx.height = static_cast<uint32_t>(rc.bottom - rc.top);
        ctx.hdc = hdc;
        ctx.context = g_getCurrentContext ? g_getCurrentContext() : nullptr;

        Frame::Present(ctx, reinterpret_cast<uintptr_t>(window));
    }

    return g_swapBuffers(hdc);
}

}

Status InstallOpenGLFrame(Layers& layers) {
    void* target = GLBackend::Get().Function(OpenGL::SwapBuffers);
    if (!target) return Status::APIUnavailable;

    HMODULE mod = GetModuleHandleW(L"opengl32.dll");
    g_getCurrentContext = mod ? reinterpret_cast<GetCurrentContext_t>(GetProcAddress(mod, "wglGetCurrentContext")) : nullptr;

    return layers.Add(target, &SwapBuffers, &g_swapBuffers);
}

void ResetOpenGLFrame() {
    g_swapBuffers = nullptr;
    g_getCurrentContext = nullptr;
}

}}
