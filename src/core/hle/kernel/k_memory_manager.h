// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/k_page_heap.h"
#include "core/hle/kernel/k_page_manager.h"

namespace Memory {
class MemorySystem;
}

namespace Kernel {

class KernelSystem;

struct FcramLayout {
    u32 application_addr;
    u32 application_size;
    u32 system_addr;
    u32 system_size;
    u32 base_addr;
    u32 base_size;
};

class KMemoryManager {
public:
    explicit KMemoryManager(KernelSystem& kernel, Memory::MemorySystem& memory)
        : m_kernel{kernel}, m_application_heap{memory}, m_system_heap{memory}, m_base_heap{memory},
          m_page_manager{memory, this} {}
    ~KMemoryManager() = default;

    void Initialize(FcramLayout* layout, u32 fcram_addr, u32 fcram_size);

    KPageHeap& GetApplicationHeap() noexcept {
        return m_application_heap;
    }

    KPageHeap& GetSystemHeap() noexcept {
        return m_system_heap;
    }

    KPageHeap& GetBaseHeap() noexcept {
        return m_base_heap;
    }

    VAddr AllocateContiguous(u32 num_pages, u32 page_alignment, MemoryOperation op);
    KPageHeap::BlockList AllocateBackwards(u32 num_pages, MemoryOperation op);
    void Free(u32 addr, u32 num_pages);

private:
    KernelSystem& m_kernel;
    KPageHeap m_application_heap;
    KPageHeap m_system_heap;
    KPageHeap m_base_heap;
    KPageManager m_page_manager;
};

} // namespace Kernel
