#include "pch.h"
#include "internal.h"

namespace VeloHook {

static std::mutex g_wndMtx;
static HWND g_wnd{};
static WNDPROC g_original{};
static std::atomic<std::shared_ptr<const WndProcCallback>> g_callback;
static std::atomic<bool> g_blockInput{ false };

static bool IsInputMessage(UINT msg) {
    return msg == WM_INPUT
        || (msg >= WM_KEYFIRST && msg <= WM_KEYLAST)
        || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST);
}

static void Detach(HWND hwnd) {
    std::lock_guard lk(g_wndMtx);

    if (g_wnd != hwnd) return;

    g_wnd = nullptr;
    g_callback.store(nullptr, std::memory_order_release);
    g_blockInput = false;
}

static LRESULT CALLBACK VeloWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HookScope scope;

    WNDPROC original = g_original;

    if (msg == WM_NCDESTROY) {
        Detach(hwnd);
        return CallWindowProcW(original, hwnd, msg, wParam, lParam);
    }

    if (!Internal::IsShuttingDown()) {
        auto callback = g_callback.load(std::memory_order_acquire);
        if (callback && *callback && (*callback)(hwnd, msg, wParam, lParam)) return TRUE;

        if (g_blockInput.load(std::memory_order_relaxed) && IsInputMessage(msg)) {
            return msg == WM_INPUT ? DefWindowProcW(hwnd, msg, wParam, lParam) : 0;
        }
    }

    return original ? CallWindowProcW(original, hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

Status HookWndProc(WndProcCallback callback, HWND window) {
    std::lock_guard lk(g_wndMtx);

    if (!window) window = Internal::GameWindow();
    if (!window) window = Internal::FindMainWindow();
    if (!window || !IsWindow(window)) return Status::InvalidParameter;

    if (g_wnd) {
        if (g_wnd != window) return Status::HookAlreadyExists;
        g_callback.store(std::make_shared<const WndProcCallback>(std::move(callback)), std::memory_order_release);
        return Status::Success;
    }

    WNDPROC original = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC));
    if (!original) return Status::FunctionNotFound;

    g_original = original;
    g_callback.store(std::make_shared<const WndProcCallback>(std::move(callback)), std::memory_order_release);
    g_wnd = window;

    SetLastError(0);
    if (!SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(VeloWndProc)) && GetLastError()) {
        g_wnd = nullptr;
        g_callback.store(nullptr, std::memory_order_release);
        return Status::MemoryProtectionFailed;
    }

    return Status::Success;
}

Status UnhookWndProc() {
    std::lock_guard lk(g_wndMtx);

    if (!g_wnd) return Status::HookNotFound;

    if (IsWindow(g_wnd)) SetWindowLongPtrW(g_wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original));

    g_wnd = nullptr;
    g_callback.store(nullptr, std::memory_order_release);
    g_blockInput = false;
    return Status::Success;
}

void SetInputBlocked(bool blocked) {
    g_blockInput.store(blocked, std::memory_order_relaxed);
}

bool IsInputBlocked() {
    return g_blockInput.load(std::memory_order_relaxed);
}

}
