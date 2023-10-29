// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_memory_manager.h"
#include "core/hle/kernel/k_page_manager.h"
#include "core/memory.h"

namespace Kernel {

void KPageManager::Initialize(VAddr start_addr, VAddr num_pages) {
    // Initialize page manager address range.
    m_start_addr = start_addr;
    m_num_pages = num_pages;

    // Compute the number of pages to allocate from the base heap.
    const u32 num_ref_counts_pages = ((sizeof(u32) * num_pages - 1) >> Memory::CITRA_PAGE_BITS) + 1;
    auto& base_heap = m_memory_manager->GetBaseHeap();

    // Allocate page refcounting memory.
    u32 ref_counts_addr{};
    {
        // KLightScopedMutex m{m_mutex};
        m_kernel_memory_usage += num_ref_counts_pages << Memory::CITRA_PAGE_BITS;
        ref_counts_addr = base_heap.AllocateContiguous(num_ref_counts_pages, 0);
        m_page_ref_counts = m_memory.GetPointer<u32>(ref_counts_addr);
        ASSERT(m_page_ref_counts);
    }

    // Zero-initialize reference counts.
    if (num_pages) {
        std::memset(m_page_ref_counts, 0, num_ref_counts_pages << Memory::CITRA_PAGE_BITS);
    }

    // Track allocated pages.
    this->Open(ref_counts_addr, num_ref_counts_pages);
}

void KPageManager::Open(VAddr addr, u32 num_pages) {
    // KLightScopedMutex m{m_mutex};

    // Increment page reference counts.
    const u32 page_start = (addr - m_start_addr) >> Memory::CITRA_PAGE_BITS;
    const u32 page_end = num_pages + page_start;
    for (u32 page = page_start; page < page_end; page++) {
        m_page_ref_counts[page_start]++;
    }
}

void KPageManager::Close(VAddr addr, u32 num_pages, MemoryOperation op) {
    // Ensure the provided address is in range.
    const u32 page_start = (addr - m_start_addr) >> Memory::CITRA_PAGE_BITS;
    const u32 page_end = page_start + num_pages;
    if (page_start >= page_end) [[unlikely]] {
        return;
    }

    // Retrieve page heaps from the memory manager.
    auto& application_heap = m_memory_manager->GetApplicationHeap();
    auto& base_heap = m_memory_manager->GetBaseHeap();
    auto& system_heap = m_memory_manager->GetSystemHeap();

    // Frees the range of pages provided from the appropriate heap.
    const auto FreePages = [&](u32 start_page, u32 num_pages) {
        const u32 current_addr = m_start_addr + (start_page << Memory::CITRA_PAGE_BITS);
        if (base_heap.Contains(current_addr)) {
            base_heap.FreeBlock(current_addr, num_pages);
        } else if (system_heap.Contains(current_addr)) {
            system_heap.FreeBlock(current_addr, num_pages);
        } else {
            application_heap.FreeBlock(current_addr, num_pages);
        }
        // Update kernel memory usage if requested.
        if (True(op & MemoryOperation::Kernel)) {
            m_kernel_memory_usage -= num_pages << Memory::CITRA_PAGE_BITS;
        }
    };

    // Iterate over the range of pages to free.
    u32 start_free_page = 0;
    u32 num_pages_to_free = 0;
    for (u32 page = page_start; page < page_end; page++) {
        const u32 new_count = --m_page_ref_counts[page];
        if (new_count) {
            // Nothing to free, continue to next page.
            if (num_pages_to_free <= 0) {
                continue;
            }
            // Free accumulated pages and reset.
            FreePages(start_free_page, num_pages_to_free);
            num_pages_to_free = 0;
        } else if (num_pages_to_free <= 0) {
            start_free_page = page;
            num_pages_to_free = 1;
        } else {
            // Advance number of pages to free.
            num_pages_to_free++;
        }
    }

    // Free any remaining pages.
    if (num_pages_to_free > 0) {
        FreePages(start_free_page, num_pages_to_free);
    }
}

} // namespace Kernel
