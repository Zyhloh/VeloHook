#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

namespace {

using VkResult = int32_t;
using VkDevice = void*;
using VkQueue = void*;
using VkSwapchainKHR = uint64_t;

constexpr VkResult VK_SUCCESS = 0;

struct VkExtent2D {
    uint32_t width;
    uint32_t height;
};

struct VkSwapchainCreateInfoKHR {
    int32_t sType;
    const void* pNext;
    uint32_t flags;
    uint64_t surface;
    uint32_t minImageCount;
    int32_t imageFormat;
    int32_t imageColorSpace;
    VkExtent2D imageExtent;
    uint32_t imageArrayLayers;
    uint32_t imageUsage;
    int32_t imageSharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;
    int32_t preTransform;
    int32_t compositeAlpha;
    int32_t presentMode;
    uint32_t clipped;
    VkSwapchainKHR oldSwapchain;
};

struct VkPresentInfoKHR {
    int32_t sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const uint64_t* pWaitSemaphores;
    uint32_t swapchainCount;
    const VkSwapchainKHR* pSwapchains;
    const uint32_t* pImageIndices;
    VkResult* pResults;
};

using QueuePresent_t = VkResult(__stdcall*)(VkQueue, const VkPresentInfoKHR*);
using AcquireNextImage_t = VkResult(__stdcall*)(VkDevice, VkSwapchainKHR, uint64_t, uint64_t, uint64_t, uint32_t*);
using CreateSwapchain_t = VkResult(__stdcall*)(VkDevice, const VkSwapchainCreateInfoKHR*, const void*, VkSwapchainKHR*);
using DestroySwapchain_t = void(__stdcall*)(VkDevice, VkSwapchainKHR, const void*);
using GetSwapchainImages_t = VkResult(__stdcall*)(VkDevice, VkSwapchainKHR, uint32_t*, void*);

struct Swapchain {
    VkSwapchainKHR handle;
    VkDevice device;
    int32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t imageCount;
};

QueuePresent_t g_queuePresent{};
AcquireNextImage_t g_acquireNextImage{};
CreateSwapchain_t g_createSwapchain{};
DestroySwapchain_t g_destroySwapchain{};
GetSwapchainImages_t g_getSwapchainImages{};

std::mutex g_mtx;
std::vector<Swapchain> g_swapchains;
HWND g_window{};

Swapchain* Find(VkSwapchainKHR handle) {
    for (auto& sc : g_swapchains) {
        if (sc.handle == handle) return &sc;
    }
    return nullptr;
}

Swapchain& Track(VkSwapchainKHR handle) {
    if (Swapchain* sc = Find(handle)) return *sc;
    return g_swapchains.emplace_back(Swapchain{ handle });
}

HWND Window() {
    if (!g_window || !IsWindow(g_window)) g_window = FindMainWindow();
    return g_window;
}

bool Describe(VkSwapchainKHR handle, FrameContext& ctx) {
    if (!handle) return false;

    std::lock_guard lk(g_mtx);

    Swapchain& sc = Track(handle);
    if (!sc.imageCount && sc.device && g_getSwapchainImages) {
        g_getSwapchainImages(sc.device, handle, &sc.imageCount, nullptr);
    }

    ctx.api = GraphicsAPI::Vulkan;
    ctx.window = Window();
    ctx.width = sc.width;
    ctx.height = sc.height;
    ctx.bufferCount = sc.imageCount;
    ctx.device = sc.device;
    ctx.vkSwapchain = handle;
    ctx.vkFormat = sc.format;

    RECT rc{};
    if ((!ctx.width || !ctx.height) && ctx.window && GetClientRect(ctx.window, &rc)) {
        ctx.width = static_cast<uint32_t>(rc.right - rc.left);
        ctx.height = static_cast<uint32_t>(rc.bottom - rc.top);
    }

    return true;
}

void BeginResize(VkSwapchainKHR handle) {
    FrameContext ctx{};
    if (Describe(handle, ctx)) Frame::ResizeBegin(ctx, handle);
}

VkResult __stdcall QueuePresent(VkQueue queue, const VkPresentInfoKHR* info) {
    HookScope scope;

    if (!info || !info->swapchainCount || !info->pSwapchains) return g_queuePresent(queue, info);

    VkPresentInfoKHR present = *info;

    FrameContext ctx{};
    if (Describe(present.pSwapchains[0], ctx)) {
        ctx.commandQueue = queue;
        ctx.bufferIndex = present.pImageIndices ? present.pImageIndices[0] : 0;
        ctx.vkPresentInfo = &present;
        Frame::Present(ctx, present.pSwapchains[0]);
    }

    return g_queuePresent(queue, &present);
}

VkResult __stdcall AcquireNextImage(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, uint64_t semaphore, uint64_t fence, uint32_t* index) {
    HookScope scope;

    if (swapchain) {
        std::lock_guard lk(g_mtx);
        Swapchain& sc = Track(swapchain);
        if (!sc.device) sc.device = device;
    }

    return g_acquireNextImage(device, swapchain, timeout, semaphore, fence, index);
}

VkResult __stdcall CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* info, const void* allocator, VkSwapchainKHR* swapchain) {
    HookScope scope;

    if (info && info->oldSwapchain) BeginResize(info->oldSwapchain);

    VkResult result = g_createSwapchain(device, info, allocator, swapchain);

    if (result == VK_SUCCESS && info && swapchain && *swapchain) {
        std::lock_guard lk(g_mtx);
        Swapchain& sc = Track(*swapchain);
        sc.device = device;
        sc.format = info->imageFormat;
        sc.width = info->imageExtent.width;
        sc.height = info->imageExtent.height;
        sc.imageCount = 0;
    }

    return result;
}

void __stdcall DestroySwapchain(VkDevice device, VkSwapchainKHR swapchain, const void* allocator) {
    HookScope scope;

    if (swapchain) {
        BeginResize(swapchain);

        std::lock_guard lk(g_mtx);
        if (Swapchain* sc = Find(swapchain)) g_swapchains.erase(g_swapchains.begin() + (sc - g_swapchains.data()));
    }

    g_destroySwapchain(device, swapchain, allocator);
}

template <typename Fn>
Status Optional(Layers& layers, Vulkan::Function fn, Fn detour, Fn* next) {
    void* target = VKBackend::Get().Function(fn);
    return target ? layers.Add(target, detour, next) : Status::Success;
}

}

Status InstallVulkanFrame(Layers& layers) {
    HMODULE loader = GetModuleHandleW(L"vulkan-1.dll");
    g_getSwapchainImages = loader ? reinterpret_cast<GetSwapchainImages_t>(GetProcAddress(loader, "vkGetSwapchainImagesKHR")) : nullptr;

    void* present = VKBackend::Get().Function(Vulkan::QueuePresentKHR);
    if (!present) return Status::APIUnavailable;

    Status st = layers.Add(present, &QueuePresent, &g_queuePresent);
    if (st != Status::Success) return st;

    st = Optional(layers, Vulkan::AcquireNextImageKHR, &AcquireNextImage, &g_acquireNextImage);
    if (st != Status::Success) return st;

    st = Optional(layers, Vulkan::CreateSwapchainKHR, &CreateSwapchain, &g_createSwapchain);
    if (st != Status::Success) return st;

    return Optional(layers, Vulkan::DestroySwapchainKHR, &DestroySwapchain, &g_destroySwapchain);
}

void ResetVulkanFrame() {
    g_queuePresent = nullptr;
    g_acquireNextImage = nullptr;
    g_createSwapchain = nullptr;
    g_destroySwapchain = nullptr;
    g_getSwapchainImages = nullptr;

    std::lock_guard lk(g_mtx);
    g_swapchains.clear();
    g_window = nullptr;
}

}}
