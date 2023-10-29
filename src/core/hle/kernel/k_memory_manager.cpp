// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_memory_manager.h"

namespace Kernel {

void KMemoryManager::Initialize(FcramLayout* layout, u32 fcram_addr, u32 fcram_size) {
    m_application_heap.Initialize(layout->application_addr, layout->application_size);
    m_system_heap.Initialize(layout->system_addr, layout->system_size);
    m_base_heap.Initialize(layout->base_addr, layout->base_size);
    m_page_manager.Initialize(fcram_addr, fcram_size >> Memory::CITRA_PAGE_BITS);
}

VAddr KMemoryManager::AllocateContiguous(u32 num_pages, u32 page_alignment, MemoryOperation op) {
    // KLightScopedMutex m{m_page_manager.GetMutex()};

    if (True(op & MemoryOperation::Kernel)) {
        m_page_manager.GetKernelMemoryUsage() += num_pages << Memory::CITRA_PAGE_BITS;
    }

    switch (op & MemoryOperation::RegionMask) {
    case MemoryOperation::RegionApplication:
        return m_application_heap.AllocateContiguous(num_pages, page_alignment);
    case MemoryOperation::RegionSystem:
        return m_system_heap.AllocateContiguous(num_pages, page_alignment);
    case MemoryOperation::RegionBase:
        return m_base_heap.AllocateContiguous(num_pages, page_alignment);
    default:
        UNREACHABLE();
        return 0;
    }
}

KPageHeap::BlockList KMemoryManager::AllocateBackwards(u32 num_pages, MemoryOperation op) {
    // KLightScopedMutex m{m_page_manager.GetMutex()};

    if (True(op & MemoryOperation::Kernel)) {
        m_page_manager.GetKernelMemoryUsage() += num_pages << Memory::CITRA_PAGE_BITS;
    }

    switch (op & MemoryOperation::RegionMask) {
    case MemoryOperation::RegionApplication:
        return m_application_heap.AllocateBackwards(num_pages);
    case MemoryOperation::RegionSystem:
        return m_system_heap.AllocateBackwards(num_pages);
    case MemoryOperation::RegionBase:
        return m_base_heap.AllocateBackwards(num_pages);
    default:
        UNREACHABLE();
        return {};
    }
}

void KMemoryManager::Free(u32 addr, u32 num_pages) {
    // KLightScopedMutex m{m_page_manager.GetMutex()};
    m_page_manager.Close(addr, num_pages, MemoryOperation::None);
}

} // namespace Kernel
