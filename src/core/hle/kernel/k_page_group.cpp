// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_page_group.h"
#include "core/hle/kernel/k_page_manager.h"

namespace Kernel {

KPageGroup::~KPageGroup() {
    EraseAll();
}

void KPageGroup::AddRange(u32 addr, u32 num_pages) {
    // If the provided range is empty there is nothing to do.
    if (num_pages == 0 || addr + (num_pages << Memory::CITRA_PAGE_BITS) == 0) {
        return;
    }

    // KScopedSchedulerLock lk{m_kernel};

    // Attempt to coaelse with last block if possible.
    if (!m_blocks.empty()) {
        KBlockInfo& last = m_blocks.back();
        if (addr != 0 && addr == last.GetEndAddress()) {
            last.m_num_pages += num_pages;
            return;
        }
    }

    // Allocate and initialize the new block.
    KBlockInfo* new_block = KBlockInfo::Allocate(m_kernel);
    new_block->Initialize(addr, num_pages);

    // Push the block to the list.
    m_blocks.push_back(*new_block);
}

void KPageGroup::IncrefPages() {
    // Iterate over block list and increment page reference counts.
    for (const auto& block : m_blocks) {
        m_page_manager->Open(block.GetAddress(), block.GetNumPages());
    }
}

u32 KPageGroup::GetTotalNumPages() {
    // Iterate over block list and count number of pages.
    u32 total_num_pages{};
    for (const auto& block : m_blocks) {
        total_num_pages = block.GetNumPages();
    }
    return total_num_pages;
}

void KPageGroup::EraseAll() {
    // Free all blocks referenced in the linked list.
    auto it = m_blocks.begin();
    while (it != m_blocks.end()) {
        KBlockInfo::Free(m_kernel, std::addressof(*it));
        it = m_blocks.erase(it);
    }
}

bool KPageGroup::IsEquivalentTo(const KPageGroup& rhs) const {
    auto lit = m_blocks.begin();
    auto rit = rhs.m_blocks.begin();
    auto lend = m_blocks.end();
    auto rend = rhs.m_blocks.end();

    while (lit != lend && rit != rend) {
        if (*lit != *rit) {
            return false;
        }

        ++lit;
        ++rit;
    }

    return lit == lend && rit == rend;
}

} // namespace Kernel
