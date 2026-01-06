# VeloHook

<p align="center">
  <b>A lightweight VTable hooking library for DirectX 11/12 games</b>
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

1. Open `VeloHook.vcxproj` in Visual Studio 2022
2. Select **Release** configuration
3. Build for **x64** or **Win32**
4. Output: `VeloHook.lib`

### Using in Your Project

1. Add `include/VeloHook.h` to your include path
2. Link against `VeloHook.lib`
3. Required libraries (auto-linked): `d3d11.lib`, `d3d12.lib`, `dxgi.lib`

---

## Quick Start

```cpp
#include "VeloHook.h"

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
Present_t oPresent = nullptr;

HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    // Your rendering code here
    
    return oPresent(sc, sync, flags);
}

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
    
    // 5. Hook functions
    VeloHook::HookSwapChain(VeloHook::SwapChain::Present, hkPresent, (void**)&oPresent);
    
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
| `Initialize()` | Initialize VeloHook and set up VTables |

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

### Accessors

| Function | Description |
|----------|-------------|
| `GetSwapChainVTable()` | Get swap chain VTable pointer |
| `GetDeviceVTable()` | Get device VTable pointer |
| `GetContextVTable()` | Get context VTable (DX11) |
| `GetCommandQueueVTable()` | Get command queue VTable (DX12) |
| `GetWindow()` | Get game window handle |
| `GetSwapChain()` | Get swap chain pointer |
| `GetDevice()` | Get device pointer |
| `GetContext()` | Get context pointer (DX11) |
| `GetCommandQueue()` | Get command queue pointer (DX12) |

### Helpers

| Function | Description |
|----------|-------------|
| `QuerySwapChainAPI(sc)` | Detect DX11/DX12 from swap chain |
| `GetDeviceFromSwapChain(sc)` | Get device from swap chain |
| `GetContextFromDevice(dev)` | Get immediate context (DX11) |
| `GetBackBuffer(sc, index)` | Get back buffer texture |
| `CreateRenderTargetView(dev, bb)` | Create RTV for rendering |
| `ReleaseRenderTargetView(rtv)` | Release RTV |
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
    Unknown = -1
};
```

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---
