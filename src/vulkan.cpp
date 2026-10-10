#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

namespace {

using VkResult = int32_t;
using VkInstance = void*;
using VkPhysicalDevice = void*;
using VkDevice = void*;

constexpr VkResult VK_SUCCESS = 0;
constexpr uint32_t kApiVersion10 = 1u << 22;
constexpr uint32_t kQueueGraphicsBit = 0x1;
constexpr uint32_t kDeviceTypeDiscrete = 2;

enum StructureType : int32_t {
    ApplicationInfo = 0,
    InstanceCreateInfo = 1,
    DeviceQueueCreateInfo = 2,
    DeviceCreateInfo = 3
};

struct VkApplicationInfo {
    StructureType sType;
    const void* pNext;
    const char* pApplicationName;
    uint32_t applicationVersion;
    const char* pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    StructureType sType;
    const void* pNext;
    uint32_t flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkDeviceQueueCreateInfo {
    StructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueFamilyIndex;
    uint32_t queueCount;
    const float* pQueuePriorities;
};

struct VkDeviceCreateInfo {
    StructureType sType;
    const void* pNext;
    uint32_t flags;
    uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
};

struct VkQueueFamilyProperties {
    uint32_t queueFlags;
    uint32_t queueCount;
    uint32_t timestampValidBits;
    uint32_t minImageTransferGranularity[3];
};

struct VkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    alignas(8) unsigned char rest[2048];
};

using PFN_vkVoidFunction = void(__stdcall*)();
using PFN_vkGetInstanceProcAddr = PFN_vkVoidFunction(__stdcall*)(VkInstance, const char*);
using PFN_vkGetDeviceProcAddr = PFN_vkVoidFunction(__stdcall*)(VkDevice, const char*);
using PFN_vkCreateInstance = VkResult(__stdcall*)(const VkInstanceCreateInfo*, const void*, VkInstance*);
using PFN_vkEnumeratePhysicalDevices = VkResult(__stdcall*)(VkInstance, uint32_t*, VkPhysicalDevice*);
using PFN_vkGetPhysicalDeviceProperties = void(__stdcall*)(VkPhysicalDevice, VkPhysicalDeviceProperties*);
using PFN_vkGetPhysicalDeviceQueueFamilyProperties = void(__stdcall*)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties*);
using PFN_vkCreateDevice = VkResult(__stdcall*)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*);

constexpr const char* kFunctions[Vulkan::Count] = {
    "vkQueuePresentKHR",
    "vkAcquireNextImageKHR",
    "vkCreateSwapchainKHR",
    "vkDestroySwapchainKHR",
    "vkQueueSubmit",
    "vkGetDeviceQueue"
};

template <typename T>
T InstanceProc(PFN_vkGetInstanceProcAddr gipa, VkInstance instance, const char* name) {
    return reinterpret_cast<T>(gipa(instance, name));
}

}

VKBackend& VKBackend::Get() {
    static VKBackend inst;
    return inst;
}

bool VKBackend::Setup(bool load) {
    std::lock_guard lk(m_mtx);

    if (m_ready) return true;

    HMODULE mod = load ? LoadLibraryW(L"vulkan-1.dll") : GetModuleHandleW(L"vulkan-1.dll");
    if (!mod) return false;

    if (!Create(mod)) {
        Reset();
        return false;
    }

    m_ready = true;
    return true;
}

bool VKBackend::Create(HMODULE mod) {
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(mod, "vkGetInstanceProcAddr"));
    if (!gipa) return false;

    auto createInstance = InstanceProc<PFN_vkCreateInstance>(gipa, nullptr, "vkCreateInstance");
    if (!createInstance) return false;

    const char* instanceExts[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };

    VkApplicationInfo app{ ApplicationInfo };
    app.apiVersion = kApiVersion10;

    VkInstanceCreateInfo ici{ InstanceCreateInfo };
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<uint32_t>(std::size(instanceExts));
    ici.ppEnabledExtensionNames = instanceExts;

    if (createInstance(&ici, nullptr, &m_instance) != VK_SUCCESS) return false;

    m_destroyInstance = InstanceProc<DestroyFn>(gipa, m_instance, "vkDestroyInstance");
    m_destroyDevice = InstanceProc<DestroyFn>(gipa, m_instance, "vkDestroyDevice");
    auto enumerate = InstanceProc<PFN_vkEnumeratePhysicalDevices>(gipa, m_instance, "vkEnumeratePhysicalDevices");
    auto properties = InstanceProc<PFN_vkGetPhysicalDeviceProperties>(gipa, m_instance, "vkGetPhysicalDeviceProperties");
    auto families = InstanceProc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(gipa, m_instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    auto createDevice = InstanceProc<PFN_vkCreateDevice>(gipa, m_instance, "vkCreateDevice");
    auto gdpa = InstanceProc<PFN_vkGetDeviceProcAddr>(gipa, m_instance, "vkGetDeviceProcAddr");

    if (!m_destroyInstance || !m_destroyDevice || !enumerate || !properties || !families || !createDevice || !gdpa) return false;

    uint32_t count = 0;
    if (enumerate(m_instance, &count, nullptr) < 0 || !count) return false;

    std::vector<VkPhysicalDevice> gpus(count);
    if (enumerate(m_instance, &count, gpus.data()) < 0 || !count) return false;

    VkPhysicalDevice gpu = gpus[0];
    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props{};
        properties(gpus[i], &props);
        if (props.deviceType == kDeviceTypeDiscrete) {
            gpu = gpus[i];
            break;
        }
    }

    uint32_t familyCount = 0;
    families(gpu, &familyCount, nullptr);
    if (!familyCount) return false;

    std::vector<VkQueueFamilyProperties> familyProps(familyCount);
    families(gpu, &familyCount, familyProps.data());

    uint32_t family = 0;
    for (uint32_t i = 0; i < familyCount; ++i) {
        if (familyProps[i].queueFlags & kQueueGraphicsBit) {
            family = i;
            break;
        }
    }

    const float priority = 1.0f;
    const char* deviceExts[] = { "VK_KHR_swapchain" };

    VkDeviceQueueCreateInfo qci{ DeviceQueueCreateInfo };
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci{ DeviceCreateInfo };
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(std::size(deviceExts));
    dci.ppEnabledExtensionNames = deviceExts;

    if (createDevice(gpu, &dci, nullptr, &m_device) != VK_SUCCESS) return false;

    m_gdpa = gdpa;

    for (uint32_t i = 0; i < Vulkan::Count; ++i) {
        m_fns[i] = reinterpret_cast<void*>(gdpa(m_device, kFunctions[i]));
    }

    return m_fns[Vulkan::QueuePresentKHR] != nullptr;
}

void VKBackend::Reset() {
    if (m_device && m_destroyDevice) m_destroyDevice(m_device, nullptr);
    if (m_instance && m_destroyInstance) m_destroyInstance(m_instance, nullptr);

    m_device = nullptr;
    m_instance = nullptr;
    m_destroyDevice = nullptr;
    m_destroyInstance = nullptr;
    m_gdpa = nullptr;
    std::fill(std::begin(m_fns), std::end(m_fns), nullptr);
    m_ready = false;
}

void VKBackend::Release() {
    std::lock_guard lk(m_mtx);
    Reset();
}

void* VKBackend::Function(Vulkan::Function fn) {
    if (fn >= Vulkan::Count || !Setup(false)) return nullptr;
    return m_fns[fn];
}

void* VKBackend::DeviceProc(const char* name) {
    if (!Setup(false)) return nullptr;
    return reinterpret_cast<void*>(m_gdpa(m_device, name));
}

}}
