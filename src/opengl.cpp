#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

static constexpr const char* kExports[OpenGL::Count] = {
    "wglSwapBuffers",
    "wglSwapLayerBuffers",
    "wglMakeCurrent",
    "wglCreateContext",
    "wglDeleteContext"
};

GLBackend& GLBackend::Get() {
    static GLBackend inst;
    return inst;
}

bool GLBackend::Setup(bool load) {
    std::lock_guard lk(m_mtx);

    if (m_ready) return true;

    HMODULE mod = load ? LoadLibraryW(L"opengl32.dll") : GetModuleHandleW(L"opengl32.dll");
    if (!mod) return false;

    for (uint32_t i = 0; i < OpenGL::Count; ++i) {
        m_fns[i] = reinterpret_cast<void*>(GetProcAddress(mod, kExports[i]));
    }

    m_ready = m_fns[OpenGL::SwapBuffers] != nullptr;
    return m_ready;
}

void GLBackend::Release() {
    std::lock_guard lk(m_mtx);

    std::fill(std::begin(m_fns), std::end(m_fns), nullptr);
    m_ready = false;
}

void* GLBackend::Function(OpenGL::Function fn) {
    if (fn >= OpenGL::Count || !Setup(false)) return nullptr;
    return m_fns[fn];
}

}}
