#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

VTHook& VTHook::Get() {
    static VTHook inst;
    return inst;
}

static volatile void* SwapPtr(void** addr, void* newVal) {
    return InterlockedExchangePointer(addr, newVal);
}

bool VTHook::Hook(void** vt, uint32_t idx, void* detour, void** orig) {
    if (!vt || !detour) return false;
    
    std::lock_guard<std::mutex> lk(m_mtx);
    
    for (const auto& h : m_hooks) {
        if (h.vtable == vt && h.index == idx) {
            return false;
        }
    }
    
    void* target = vt[idx];
    if (orig) *orig = target;
    
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(&vt[idx], &mbi, sizeof(mbi))) {
        return false;
    }
    
    DWORD old;
    if (!VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &old)) {
        return false;
    }
    
    SwapPtr(&vt[idx], detour);
    
    DWORD tmp;
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, old, &tmp);
    
    VTableHook entry;
    entry.vtable = vt;
    entry.index = idx;
    entry.original = target;
    entry.detour = detour;
    
    m_hooks.push_back(entry);
    return true;
}

bool VTHook::Unhook(void** vt, uint32_t idx) {
    std::lock_guard<std::mutex> lk(m_mtx);
    
    for (auto it = m_hooks.begin(); it != m_hooks.end(); ++it) {
        if (it->vtable == vt && it->index == idx) {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(&vt[idx], &mbi, sizeof(mbi))) {
                return false;
            }
            
            DWORD old;
            if (!VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &old)) {
                return false;
            }
            
            SwapPtr(&vt[idx], it->original);
            
            DWORD tmp;
            VirtualProtect(mbi.BaseAddress, mbi.RegionSize, old, &tmp);
            
            m_hooks.erase(it);
            return true;
        }
    }
    
    return false;
}

bool VTHook::UnhookAll() {
    std::lock_guard<std::mutex> lk(m_mtx);
    
    for (auto& h : m_hooks) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(&h.vtable[h.index], &mbi, sizeof(mbi))) {
            DWORD old;
            if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &old)) {
                SwapPtr(&h.vtable[h.index], h.original);
                DWORD tmp;
                VirtualProtect(mbi.BaseAddress, mbi.RegionSize, old, &tmp);
            }
        }
    }
    
    m_hooks.clear();
    return true;
}

void* VTHook::Original(void** vt, uint32_t idx) {
    std::lock_guard<std::mutex> lk(m_mtx);
    
    for (const auto& h : m_hooks) {
        if (h.vtable == vt && h.index == idx) {
            return h.original;
        }
    }
    
    return nullptr;
}

void VTHook::EnterHook(void** vt, uint32_t idx) {
    std::lock_guard<std::mutex> lk(m_mtx);
    
    for (auto& h : m_hooks) {
        if (h.vtable == vt && h.index == idx) {
            InterlockedIncrement(reinterpret_cast<volatile long*>(&h.refCount));
            return;
        }
    }
}

void VTHook::LeaveHook(void** vt, uint32_t idx) {
    std::lock_guard<std::mutex> lk(m_mtx);
    
    for (auto& h : m_hooks) {
        if (h.vtable == vt && h.index == idx) {
            InterlockedDecrement(reinterpret_cast<volatile long*>(&h.refCount));
            return;
        }
    }
}

bool VTHook::WaitForHook(void** vt, uint32_t idx, uint32_t timeoutMs) {
    auto start = GetTickCount64();
    
    while (true) {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            for (const auto& h : m_hooks) {
                if (h.vtable == vt && h.index == idx) {
                    if (h.refCount == 0) {
                        return true;
                    }
                    break;
                }
            }
        }
        
        if (GetTickCount64() - start > timeoutMs) {
            return false;
        }
        
        Sleep(1);
    }
}

bool VTHook::SafeUnhookAll(uint32_t timeoutMs) {
    m_shuttingDown.store(true, std::memory_order_release);
    
    auto start = GetTickCount64();
    
    while (true) {
        bool allClear = true;
        
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            for (const auto& h : m_hooks) {
                if (h.refCount > 0) {
                    allClear = false;
                    break;
                }
            }
        }
        
        if (allClear) break;
        
        if (GetTickCount64() - start > timeoutMs) {
            m_shuttingDown.store(false, std::memory_order_release);
            return false;
        }
        
        Sleep(1);
    }
    
    UnhookAll();
    return true;
}

}}
