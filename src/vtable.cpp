#include "pch.h"
#include "internal.h"

namespace VeloHook {
namespace Internal {

static constexpr DWORD kExecuteMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

static bool WriteSlot(void** slot, void* value) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(slot, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;

    DWORD protect = (mbi.Protect & kExecuteMask) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), protect, &old)) return false;

    InterlockedExchangePointer(slot, value);

    VirtualProtect(slot, sizeof(void*), old, &old);
    return true;
}

VTHook& VTHook::Get() {
    static VTHook inst;
    return inst;
}

VTHook::Entry* VTHook::Find(void** vt, uint32_t idx) {
    for (auto& h : m_hooks) {
        if (h.vtable == vt && h.index == idx) return &h;
    }
    return nullptr;
}

Status VTHook::Hook(void** vt, uint32_t idx, void* detour, void** orig, bool internal) {
    if (!vt || !detour || (internal && !orig)) return Status::InvalidParameter;

    std::lock_guard lk(m_mtx);

    Entry* e = Find(vt, idx);
    void* original = e ? e->original : vt[idx];

    if (internal) {
        if (e && e->layer) return Status::HookAlreadyExists;

        InterlockedExchangePointer(orig, e && e->detour ? e->detour : original);
        if (!WriteSlot(&vt[idx], detour)) return Status::MemoryProtectionFailed;

        if (!e) e = &m_hooks.emplace_back(Entry{ vt, idx, original });
        e->layer = detour;
        e->layerNext = orig;
        return Status::Success;
    }

    if (e && e->detour) return Status::HookAlreadyExists;

    if (orig) *orig = original;

    if (e && e->layer) {
        InterlockedExchangePointer(e->layerNext, detour);
    } else if (!WriteSlot(&vt[idx], detour)) {
        return Status::MemoryProtectionFailed;
    }

    if (!e) e = &m_hooks.emplace_back(Entry{ vt, idx, original });
    e->detour = detour;
    return Status::Success;
}

Status VTHook::Unhook(void** vt, uint32_t idx, bool internal) {
    std::lock_guard lk(m_mtx);

    Entry* e = Find(vt, idx);
    if (!e || !(internal ? e->layer : e->detour)) return Status::HookNotFound;

    if (internal) {
        if (!WriteSlot(&vt[idx], e->detour ? e->detour : e->original)) return Status::MemoryProtectionFailed;
        e->layer = nullptr;
        e->layerNext = nullptr;
    } else {
        if (e->layer) {
            InterlockedExchangePointer(e->layerNext, e->original);
        } else if (!WriteSlot(&vt[idx], e->original)) {
            return Status::MemoryProtectionFailed;
        }
        e->detour = nullptr;
    }

    if (!e->layer && !e->detour) {
        m_hooks.erase(m_hooks.begin() + (e - m_hooks.data()));
    }

    return Status::Success;
}

bool VTHook::UnhookAll() {
    std::lock_guard lk(m_mtx);

    bool ok = true;
    for (const auto& h : m_hooks) {
        ok &= WriteSlot(&h.vtable[h.index], h.original);
    }

    m_hooks.clear();
    return ok;
}

void* VTHook::Original(void** vt, uint32_t idx) {
    std::lock_guard lk(m_mtx);

    Entry* e = Find(vt, idx);
    return e ? e->original : nullptr;
}

}}
