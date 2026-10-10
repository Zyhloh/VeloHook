#include "pch.h"
#include "internal.h"

#include "../third_party/minhook/include/MinHook.h"

namespace VeloHook {
namespace Internal {

static Status ToStatus(MH_STATUS status) {
    switch (status) {
        case MH_OK: return Status::Success;
        case MH_ERROR_NOT_INITIALIZED: return Status::NotInitialized;
        case MH_ERROR_ALREADY_CREATED:
        case MH_ERROR_ENABLED: return Status::HookAlreadyExists;
        case MH_ERROR_NOT_CREATED:
        case MH_ERROR_DISABLED: return Status::HookNotFound;
        case MH_ERROR_NOT_EXECUTABLE:
        case MH_ERROR_UNSUPPORTED_FUNCTION: return Status::UnsupportedFunction;
        case MH_ERROR_MEMORY_ALLOC: return Status::MemoryAllocationFailed;
        case MH_ERROR_MEMORY_PROTECT: return Status::MemoryProtectionFailed;
        case MH_ERROR_MODULE_NOT_FOUND:
        case MH_ERROR_FUNCTION_NOT_FOUND: return Status::FunctionNotFound;
        default: return Status::Unknown;
    }
}

InlineHook& InlineHook::Get() {
    static InlineHook inst;
    return inst;
}

InlineHook::Entry* InlineHook::Find(void* target) {
    for (auto& h : m_hooks) {
        if (h.target == target) return &h;
    }
    return nullptr;
}

Status InlineHook::Hook(void* target, void* detour, void** orig, bool internal) {
    if (!target || !detour || (internal && !orig)) return Status::InvalidParameter;

    std::lock_guard lk(m_mtx);

    if (Entry* e = Find(target)) {
        if (internal || e->detour) return Status::HookAlreadyExists;

        if (orig) *orig = e->trampoline;
        InterlockedExchangePointer(e->layerNext, detour);
        e->detour = detour;
        return Status::Success;
    }

    if (!m_init) {
        MH_STATUS st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) return ToStatus(st);
        m_init = true;
    }

    void* trampoline = nullptr;
    MH_STATUS st = MH_CreateHook(target, detour, &trampoline);
    if (st != MH_OK) return ToStatus(st);

    if (orig) InterlockedExchangePointer(orig, trampoline);

    st = MH_EnableHook(target);
    if (st != MH_OK) {
        MH_RemoveHook(target);
        return ToStatus(st);
    }

    if (internal) {
        m_hooks.push_back({ target, trampoline, nullptr, detour, orig });
    } else {
        m_hooks.push_back({ target, trampoline, detour, nullptr, nullptr });
    }

    return Status::Success;
}

Status InlineHook::Unhook(void* target, bool internal) {
    if (!target) return Status::InvalidParameter;

    std::lock_guard lk(m_mtx);

    Entry* e = Find(target);
    if (!e || !(internal ? e->layer : e->detour)) return Status::HookNotFound;

    if (!internal && e->layer) {
        InterlockedExchangePointer(e->layerNext, e->trampoline);
        e->detour = nullptr;
        return Status::Success;
    }

    if (internal && e->detour) return Status::HooksBusy;

    MH_STATUS st = MH_DisableHook(target);
    if (st != MH_OK) return ToStatus(st);

    MH_RemoveHook(target);
    m_hooks.erase(m_hooks.begin() + (e - m_hooks.data()));
    return Status::Success;
}

bool InlineHook::DisableAll() {
    std::lock_guard lk(m_mtx);
    return !m_init || MH_DisableHook(MH_ALL_HOOKS) == MH_OK;
}

bool InlineHook::UnhookAll() {
    std::lock_guard lk(m_mtx);

    if (!m_init) return true;

    bool ok = MH_DisableHook(MH_ALL_HOOKS) == MH_OK;
    MH_Uninitialize();

    m_hooks.clear();
    m_init = false;
    return ok;
}

bool InlineHook::Active() {
    std::lock_guard lk(m_mtx);
    return m_init;
}

}}
