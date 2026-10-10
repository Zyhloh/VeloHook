#include "pch.h"
#include "internal.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace VeloHook {

using Internal::GLBackend;
using Internal::Graphics;
using Internal::InlineHook;
using Internal::VKBackend;
using Internal::VTHook;

static std::mutex g_lifecycle;
static std::atomic<bool> g_init{ false };
static std::atomic<bool> g_shuttingDown{ false };
static std::atomic<int> g_inFlight{ 0 };
static thread_local int t_depth = 0;
static bool g_restored = true;

static wchar_t g_targetProcess[MAX_PATH]{};
static bool g_consoleOpen = false;
static bool g_consoleOwned = false;

namespace Internal {

std::mutex& LifecycleMutex() {
    return g_lifecycle;
}

bool IsInitialized() {
    return g_init.load(std::memory_order_acquire);
}

bool IsShuttingDown() {
    return g_shuttingDown.load(std::memory_order_acquire);
}

}

void SetTargetProcess(const char* processName) {
    if (!processName || !MultiByteToWideChar(CP_UTF8, 0, processName, -1, g_targetProcess, MAX_PATH)) {
        g_targetProcess[0] = 0;
    }
}

void SetTargetProcessW(const wchar_t* processName) {
    if (!processName) {
        g_targetProcess[0] = 0;
        return;
    }
    wcsncpy_s(g_targetProcess, processName, _TRUNCATE);
}

bool IsTargetProcess() {
    if (!g_targetProcess[0]) return true;

    wchar_t path[MAX_PATH * 4]{};
    DWORD len = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (!len || len >= std::size(path)) return true;

    const wchar_t* name = path;
    for (const wchar_t* p = path; *p; ++p) {
        if (*p == L'\\' || *p == L'/') name = p + 1;
    }

    return _wcsicmp(name, g_targetProcess) == 0;
}

bool IsEngineReady() {
    if (!IsTargetProcess()) return false;

    bool hasDX = (GetModuleHandleW(L"d3d11.dll") || GetModuleHandleW(L"d3d12.dll")) && GetModuleHandleW(L"dxgi.dll");
    bool hasOther = GetModuleHandleW(L"vulkan-1.dll") || GetModuleHandleW(L"opengl32.dll");
    if (!hasDX && !hasOther) return false;

    return Internal::FindMainWindow() != nullptr;
}

bool WaitForEngine(uint32_t timeoutMs, uint32_t checkIntervalMs) {
    if (!IsTargetProcess()) return false;

    ULONGLONG start = GetTickCount64();

    while (!IsEngineReady()) {
        if (GetTickCount64() - start > timeoutMs) return false;
        Sleep(checkIntervalMs);
    }

    return true;
}

bool OpenConsole() {
    if (!IsTargetProcess()) return false;
    if (g_consoleOpen) return true;

    if (AllocConsole()) {
        g_consoleOwned = true;
    } else if (!GetConsoleWindow()) {
        return false;
    }

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);

    g_consoleOpen = true;
    return true;
}

void CloseConsole() {
    if (!g_consoleOpen) return;

    if (g_consoleOwned) {
        FILE* f;
        freopen_s(&f, "NUL", "w", stdout);
        freopen_s(&f, "NUL", "w", stderr);
        freopen_s(&f, "NUL", "r", stdin);
        FreeConsole();
    }

    g_consoleOpen = false;
    g_consoleOwned = false;
}

void ConsolePrint(const char* fmt, ...) {
    if (!g_consoleOpen || !fmt) return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void EnterHook() {
    ++t_depth;
    g_inFlight.fetch_add(1, std::memory_order_acq_rel);
}

void LeaveHook() {
    g_inFlight.fetch_sub(1, std::memory_order_acq_rel);
    --t_depth;
}

static bool WaitForHooks(uint32_t timeoutMs) {
    ULONGLONG start = GetTickCount64();

    while (g_inFlight.load(std::memory_order_acquire) > 0) {
        if (GetTickCount64() - start > timeoutMs) return false;
        Sleep(1);
    }

    Sleep(10);
    return true;
}

static void ReleaseBackends() {
    Graphics::Get().Release();
    GLBackend::Get().Release();
    VKBackend::Get().Release();
}

Status Initialize(GraphicsAPI api) {
    std::lock_guard lk(g_lifecycle);

    if (g_init) return Status::AlreadyInitialized;
    if (g_shuttingDown) return Status::HooksBusy;
    if (!IsTargetProcess()) return Status::WrongProcess;
    if (api == GraphicsAPI::None) return Status::InvalidParameter;

    if (!Graphics::Get().Setup(api)) {
        ReleaseBackends();
        return Status::DeviceCreationFailed;
    }

    Status st = Internal::InstallFrameHooks();
    if (st != Status::Success) {
        WaitForHooks(1000);
        ReleaseBackends();
        return st;
    }

    g_init = true;
    return Status::Success;
}

static Status ShutdownNow(uint32_t timeoutMs) {
    std::lock_guard lk(g_lifecycle);

    bool hadWndProc = UnhookWndProc() == Status::Success;
    if (!g_init && !g_shuttingDown && !InlineHook::Get().Active() && !hadWndProc) return Status::NotInitialized;

    g_shuttingDown = true;

    g_restored &= VTHook::Get().UnhookAll();
    g_restored &= InlineHook::Get().DisableAll();

    if (!WaitForHooks(timeoutMs)) return Status::HooksBusy;

    g_restored &= InlineHook::Get().UnhookAll();

    Internal::ReleaseFrameHooks();
    ReleaseBackends();

    bool ok = g_restored;
    g_restored = true;
    g_init = false;
    g_shuttingDown = false;

    return ok ? Status::Success : Status::MemoryProtectionFailed;
}

static DWORD WINAPI ShutdownThread(LPVOID param) {
    ShutdownNow(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(param)));
    return 0;
}

Status Shutdown(uint32_t timeoutMs) {
    if (t_depth > 0) {
        HANDLE thread = CreateThread(nullptr, 0, ShutdownThread, reinterpret_cast<LPVOID>(static_cast<uintptr_t>(timeoutMs)), 0, nullptr);
        if (!thread) return Status::Unknown;
        CloseHandle(thread);
        return Status::Success;
    }

    return ShutdownNow(timeoutMs);
}

static DWORD WINAPI UnloadThread(LPVOID param) {
    Status st;
    while ((st = ShutdownNow(1000)) == Status::HooksBusy) {}

    if (st != Status::Success && st != Status::NotInitialized) return 1;
    FreeLibraryAndExitThread(static_cast<HMODULE>(param), 0);
}

void Unload(HMODULE module) {
    HMODULE target = module ? module : reinterpret_cast<HMODULE>(&__ImageBase);

    HANDLE thread = CreateThread(nullptr, 0, UnloadThread, target, 0, nullptr);
    if (thread) CloseHandle(thread);
}

GraphicsAPI GetActiveAPI() {
    return g_init ? Graphics::Get().ActiveAPI() : GraphicsAPI::None;
}

}
