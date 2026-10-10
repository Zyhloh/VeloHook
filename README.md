# VeloHook

<p align="center">
  <b>A lightweight hooking library for DirectX 11/12, OpenGL and Vulkan games</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows-blue" alt="Platform">
  <img src="https://img.shields.io/badge/arch-x86%20%7C%20x64-green" alt="Architecture">
  <img src="https://img.shields.io/badge/license-MIT-yellow" alt="License">
</p>

---

## Features

- **VTable Hooking** — Hook SwapChain, Device, DeviceContext (DX11), CommandQueue (DX12)
- **Universal DX Support** — Works on both DirectX 11 and DirectX 12 games automatically
- **OpenGL & Vulkan Support** — Hook `wglSwapBuffers`, `vkQueuePresentKHR` and more through the bundled inline hook engine
- **Inline Hooking** — Hook any function or export, powered by MinHook
- **Unified Present Callback** — One `OnPresent` callback for DX11, DX12, OpenGL and Vulkan with a filled `FrameContext`
- **Game Object Capture** — The game's real swap chain, device, context and DX12 command queue, captured from its own frames
- **Resize Notifications** — `OnResize` fires before and after the back buffers change, so render targets can be released and rebuilt
- **WndProc & Input** — Window procedure hook with callback and input blocking
- **Typed Hooks** — Template overloads keep detour and original types in sync, no `void**` casts needed
- **Safe Unload** — Waits for in-flight hooks before tearing down, `Unload()` frees the module from any thread, including inside a hook
- **Process Validation** — Target specific processes to prevent accidental injection
- **Engine Ready Detection** — Wait for game engine initialization before hooking
- **Safe Console** — Console allocation that won't crash non-target processes
- **Runtime API Detection** — `QuerySwapChainAPI()` detects actual game API from swap chain
- **Helper Functions** — Get device, context, back buffer, create RTVs, and more
- **Thread Safe** — All operations are mutex protected
- **Stealthy** — Atomic pointer swaps, proper memory region protection

---

## Installation

### Building from Source

1. Clone with submodules: `git clone --recursive https://github.com/Zyhloh/VeloHook.git`
   (already cloned? run `git submodule update --init`)
2. Open `VeloHook.vcxproj` in Visual Studio
3. Select **Release** configuration
4. Build for **x64** or **Win32**
5. Output: `VeloHook.lib` (MinHook is compiled in, nothing else to link)

### Using in Your Project

1. Add `include/VeloHook.h` to your include path
2. Link against `VeloHook.lib`
3. No DirectX, OpenGL or Vulkan import libraries required (modules are resolved at runtime)

---

## Quick Start

```cpp
#include "VeloHook.h"

void Initialize(HMODULE hModule) {
    // 1. Set target process (optional but recommended)
    VeloHook::SetTargetProcess("game.exe");

    // 2. Open console safely (only works on target process)
    VeloHook::OpenConsole();
    VeloHook::ConsolePrint("[MyCheat] Starting...\n");

    // 3. Wait for engine to be ready
    if (!VeloHook::WaitForEngine(30000)) {
        VeloHook::ConsolePrint("[MyCheat] Engine not ready, exiting\n");
        VeloHook::CloseConsole();
        return;
    }

    // 4. Initialize VeloHook
    if (VeloHook::Initialize() != VeloHook::Status::Success) {
        VeloHook::ConsolePrint("[MyCheat] Init failed\n");
        VeloHook::CloseConsole();
        return;
    }

    // 5. Render every frame, whatever API the game uses
    VeloHook::OnPresent([](VeloHook::FrameContext& frame) {
        // frame.swapChain / frame.device / frame.context / frame.commandQueue are the game's objects
    });

    VeloHook::OnResize([](VeloHook::ResizeStage stage, const VeloHook::FrameContext& frame) {
        // Before: release anything holding the back buffers. After: recreate it.
    });

    VeloHook::ConsolePrint("[MyCheat] Ready!\n");
}
```

---

## API Reference

### Process Validation

| Function | Description |
|----------|-------------|
| `SetTargetProcess(name)` | Set expected process name (case-insensitive) |
| `SetTargetProcessW(name)` | Wide string version |
| `IsTargetProcess()` | Check if current process matches target |

### Engine Detection

| Function | Description |
|----------|-------------|
| `IsEngineReady()` | Check if game engine is ready for hooking |
| `WaitForEngine(timeout, interval)` | Wait for engine to be ready |

### Console (Safe)

| Function | Description |
|----------|-------------|
| `OpenConsole()` | Allocate console (only on target process) |
| `CloseConsole()` | Free console |
| `ConsolePrint(fmt, ...)` | Printf-style output |

### Lifecycle

| Function | Description |
|----------|-------------|
| `Initialize(api)` | Initialize VeloHook (`GraphicsAPI::Auto` detects DX11, DX12, Vulkan, then OpenGL) |
| `Shutdown(timeoutMs)` | Remove all hooks, wait for in-flight hooks to return, release internal resources. Returns `HooksBusy` if hooks are still running after the timeout; call again to finish |
| `Unload(module)` | Shutdown on a worker thread, then `FreeLibraryAndExitThread`. Safe to call from inside a hook. Defaults to the module VeloHook is linked into |
| `GetActiveAPI()` | Get the API VeloHook initialized with |
| `HookScope` | RAII guard that marks a thread as inside a hook. Put one at the top of your own detours so `Shutdown` waits for them |

### Frame Callbacks

| Function | Description |
|----------|-------------|
| `OnPresent(callback)` | Called once per presented frame with a `FrameContext&` (pass `nullptr` to clear) |
| `OnResize(callback)` | Called with `ResizeStage::Before` / `After` around `ResizeBuffers`, swap chain recreation and window size changes |
| `GetLastFrame(frame)` | Copy of the most recent `FrameContext` |
| `GetGameWindow()` | Window the game presents to |
| `GetGameSwapChain()` | The game's swap chain (DXGI) |
| `GetGameDevice()` | The game's `ID3D11Device` / `ID3D12Device` / `VkDevice` |
| `GetGameContext()` | The game's immediate context (DX11) / `HGLRC` (OpenGL) |
| `GetGameCommandQueue()` | The game's `ID3D12CommandQueue` / `VkQueue` |

Frame hooks are installed by `Initialize`, so the game's objects are captured from the first frame. Captured pointers are borrowed (no reference is held) and stay valid while the game's swap chain is alive.

| Field | DX11 | DX12 | OpenGL | Vulkan |
|-------|------|------|--------|--------|
| `swapChain` | `IDXGISwapChain*` | `IDXGISwapChain*` | — | — |
| `device` | `ID3D11Device*` | `ID3D12Device*` | — | `VkDevice` |
| `context` | `ID3D11DeviceContext*` | — | `HGLRC` | — |
| `commandQueue` | — | `ID3D12CommandQueue*` | — | `VkQueue` |
| `bufferIndex` | 0 | current back buffer | 0 | acquired image index |
| `hdc` | — | — | presenting DC | — |
| `vkSwapchain` / `vkFormat` | — | — | — | swapchain handle / `VkFormat` |
| `vkPresentInfo` | — | — | — | mutable copy of `VkPresentInfoKHR`, presented after the callback |

DX12 frames are delivered once the game's command queue has been captured (its first `ExecuteCommandLists`, normally within the first frame). `vkFormat` is 0 for a Vulkan swapchain created before `Initialize` until the game recreates it.

### Window & Input

| Function | Description |
|----------|-------------|
| `HookWndProc(callback, window)` | Subclass the game window (defaults to the presenting window). Return `true` from the callback to swallow a message |
| `UnhookWndProc()` | Restore the original window procedure (also done by `Shutdown`) |
| `SetInputBlocked(blocked)` | Stop keyboard and mouse messages reaching the game |
| `IsInputBlocked()` | Current block state |

### Hooking

| Function | Description |
|----------|-------------|
| `HookSwapChain(index, detour, original)` | Hook swap chain method |
| `HookDevice(index, detour, original)` | Hook device method |
| `HookContext(index, detour, original)` | Hook device context (DX11) |
| `HookCommandQueue(index, detour, original)` | Hook command queue (DX12) |
| `UnhookSwapChain(index)` | Remove swap chain hook |
| `UnhookDevice(index)` | Remove device hook |
| `UnhookContext(index)` | Remove context hook |
| `UnhookCommandQueue(index)` | Remove command queue hook |
| `HookOpenGL(fn, detour, original)` | Hook an OpenGL function (`VeloHook::OpenGL::SwapBuffers` etc.) |
| `HookVulkan(fn, detour, original)` | Hook a Vulkan function (`VeloHook::Vulkan::QueuePresentKHR` etc.) |
| `UnhookOpenGL(fn)` | Remove OpenGL hook |
| `UnhookVulkan(fn)` | Remove Vulkan hook |
| `HookFunction(target, detour, original)` | Inline hook any function address |
| `HookExport(module, name, detour, original)` | Inline hook an exported function |
| `UnhookFunction(target)` | Remove inline hook |

`HookOpenGL` / `HookVulkan` work whenever `opengl32.dll` / `vulkan-1.dll` is loaded, regardless of which API `Initialize` detected.

Your own hooks on Present, ResizeBuffers, `wglSwapBuffers` or `vkQueuePresentKHR` coexist with VeloHook's frame hooks and can be added or removed at any time.

Every hook function has a typed overload that checks the detour against the original pointer:

```cpp
using Present_t = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
Present_t oPresent = nullptr;

HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    VeloHook::HookScope scope;
    return oPresent(sc, sync, flags);
}

VeloHook::HookSwapChain(VeloHook::SwapChain::Present, hkPresent, &oPresent);
```

### Accessors

| Function | Description |
|----------|-------------|
| `GetSwapChainVTable()` | Get swap chain VTable pointer |
| `GetDeviceVTable()` | Get device VTable pointer |
| `GetContextVTable()` | Get context VTable (DX11) |
| `GetCommandQueueVTable()` | Get command queue VTable (DX12) |
| `GetOpenGLFunction(fn)` | Get resolved OpenGL function address |
| `GetVulkanFunction(fn)` | Get resolved Vulkan function address |
| `GetWindow()` | Get game window handle |
| `GetSwapChain()` | Get VeloHook's internal swap chain (use the hook's `this` for the game's) |
| `GetDevice()` | Get VeloHook's internal device |
| `GetContext()` | Get VeloHook's internal context (DX11) |
| `GetCommandQueue()` | Get VeloHook's internal command queue (DX12) |

### Helpers

| Function | Description |
|----------|-------------|
| `QuerySwapChainAPI(sc)` | Detect DX11/DX12 from swap chain |
| `GetDeviceFromSwapChain(sc)` | Get device from swap chain (caller releases) |
| `GetContextFromDevice(dev)` | Get immediate context (DX11, caller releases) |
| `GetBackBuffer(sc, index)` | Get back buffer texture (caller releases) |
| `CreateRenderTargetView(dev, bb)` | Create RTV for rendering |
| `ReleaseRenderTargetView(rtv)` | Release RTV |
| `ReleaseObject(obj)` | Release any object returned by the helpers above |
| `GetSwapChainSize(sc, w, h)` | Get render dimensions |
| `GetSwapChainWindow(sc)` | Get HWND from swap chain |
| `GetOriginalFunction(vt, index)` | Get original function pointer |

---

## VTable Indices

### SwapChain (DXGI)

```cpp
VeloHook::SwapChain::Present          // 8
VeloHook::SwapChain::GetBuffer        // 9
VeloHook::SwapChain::ResizeBuffers    // 13
VeloHook::SwapChain::ResizeTarget     // 14
VeloHook::SwapChain1::Present1        // 22
```

### Device Context (DX11)

```cpp
VeloHook::Context11::Draw             // 13
VeloHook::Context11::DrawIndexed      // 12
VeloHook::Context11::DrawInstanced    // 21
VeloHook::Context11::DrawIndexedInstanced // 20
VeloHook::Context11::VSSetShader      // 11
VeloHook::Context11::PSSetShader      // 9
```

### OpenGL

```cpp
VeloHook::OpenGL::SwapBuffers        // wglSwapBuffers
VeloHook::OpenGL::MakeCurrent        // wglMakeCurrent
```

### Vulkan

```cpp
VeloHook::Vulkan::QueuePresentKHR    // vkQueuePresentKHR
VeloHook::Vulkan::QueueSubmit        // vkQueueSubmit
VeloHook::Vulkan::CreateSwapchainKHR // vkCreateSwapchainKHR
```

### Command Queue (DX12)

```cpp
VeloHook::CommandQueue12::ExecuteCommandLists // 10
VeloHook::CommandQueue12::Signal      // 14
```

---

## Status Codes

```cpp
enum class Status {
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
```

---

## License

MIT License - See [LICENSE](LICENSE) for details.

Inline hooking is powered by [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause), included as a git submodule. If you distribute binaries, include MinHook's license notice.

---
