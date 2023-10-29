// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/scope_exit.h"
#include "core/hle/kernel/k_memory_block_manager.h"

namespace Kernel {

void KMemoryBlockManager::Initialize(u32 addr_space_start, u32 addr_space_end) {
    const u32 num_pages = (addr_space_end - addr_space_start) >> Memory::CITRA_PAGE_BITS;
    KMemoryBlock* block = KMemoryBlock::Allocate(m_kernel);
    block->Initialize(addr_space_start, num_pages, 0, KMemoryState::Free, KMemoryPermission::None);
    m_blocks.push_back(*block);
}

s64 KMemoryBlockManager::GetTotalCommittedMemory() {
    u32 total_commited_memory{};
    for (const auto& block : m_blocks) {
        const KMemoryInfo info = block.GetInfo();
        if (info.GetAddress() - 0x1C000000 >= 0x4000000 &&
            True(info.GetState() & KMemoryState::Private)) {
            total_commited_memory += info.GetSize();
        }
    }
    return total_commited_memory;
}

KMemoryBlock* KMemoryBlockManager::FindFreeBlock(VAddr start, u32 num_pages, u32 block_num_pages) {
    const VAddr end = start + (num_pages << Memory::CITRA_PAGE_BITS);
    const u32 block_size = block_num_pages << Memory::CITRA_PAGE_BITS;
    for (auto& block : m_blocks) {
        const KMemoryInfo info = block.GetInfo();
        if (info.GetState() != KMemoryState::Free) {
            continue;
        }
        const VAddr block_start = std::max(info.GetAddress(), start);
        const VAddr block_end = block_start + block_size;
        if (block_end <= end && block_end <= info.GetEndAddress()) {
            return std::addressof(block);
        }
    }
    return nullptr;
}

void KMemoryBlockManager::CoalesceBlocks() {
    auto it = m_blocks.begin();
    while (true) {
        iterator prev = it++;
        if (it == m_blocks.end()) {
            break;
        }

        // Merge adjacent blocks with the same properties.
        if (prev->HasSameProperties(*it)) {
            KMemoryBlock* block = std::addressof(*it);
            const KMemoryInfo info = block->GetInfo();
            prev->GrowBlock(info.GetAddress(), info.GetNumPages());
            KMemoryBlock::Free(m_kernel, block);
            m_blocks.erase(it);
            it = prev;
        }
    }
}

Result KMemoryBlockManager::Update(VAddr addr, u32 num_pages, KMemoryState state,
                                   KMemoryPermission perms, u32 tag) {
    // Initialize iterators.
    const VAddr last_addr = addr + (num_pages << Memory::CITRA_PAGE_BITS) - 1;
    iterator begin = FindIterator(addr);
    iterator end = FindIterator(last_addr);

    // Before returning we have to coalesce.
    SCOPE_EXIT {
        this->CoalesceBlocks();
    };

    // Begin and end addresses are in different blocks. We need to shrink/remove
    // any blocks in that range and insert a new one with the new attributes.
    if (begin != end) {
        // Any blocks in-between begin and end can be completely erased.
        for (auto it = std::next(begin); it != end; it = m_blocks.erase(it)) {
            KMemoryBlock::Free(m_kernel, std::addressof(*it));
        }

        // If begin block has same properties, grow it to accomodate the range.
        if (begin->HasProperties(state, perms, tag)) {
            begin->GrowBlock(addr, num_pages);

            // If the end block is fully overwritten, remove it.
            if (end->GetLastAddress() == last_addr) {
                KMemoryBlock::Free(m_kernel, std::addressof(*end));
                m_blocks.erase(end);
                R_SUCCEED();
            }

            // Shrink the block containing the end va
            end->ShrinkBlock(addr, num_pages);
        } else if (end->HasProperties(state, perms, tag)) {
            // If end block has same properties, grow it to accomodate the range.
            end->GrowBlock(addr, num_pages);

            // Remove start block if fully overwritten
            if (begin->GetAddress() == addr) {
                KMemoryBlock::Free(m_kernel, std::addressof(*begin));
                m_blocks.erase(begin);
                R_SUCCEED();
            }

            // Shrink the block containing the start va
            begin->ShrinkBlock(addr, num_pages);
        } else {
            // Neither begin and end blocks have required properties.
            // Shrink them both and create a new block in-between.
            if (begin->IncludesRange(addr, num_pages)) {
                KMemoryBlock::Free(m_kernel, std::addressof(*begin));
                begin = m_blocks.erase(begin);
            } else {
                // Otherwise cut off the part that inside our range
                begin->ShrinkBlock(addr, num_pages);
            }

            // If the end block is fully inside the range, remove it
            if (end->IncludesRange(addr, num_pages)) {
                KMemoryBlock::Free(m_kernel, std::addressof(*end));
                end = m_blocks.erase(end);
            } else {
                // Otherwise cut off the part that inside our range
                end->ShrinkBlock(addr, num_pages);
            }

            // The range [addr, last_addr] is now void, create new block in its place.
            KMemoryBlock* block = KMemoryBlock::Allocate(m_kernel);
            block->Initialize(addr, num_pages, 0, state, perms);

            // Insert it to the block list
            m_blocks.insert(end, *block);
        }

        R_SUCCEED();
    }

    // Start and end address are in same block, we have to split that.
    if (!begin->HasProperties(state, perms, tag)) {
        const KMemoryInfo info = begin->GetInfo();
        const u32 pages_in_block = (addr - info.GetAddress()) >> Memory::CITRA_PAGE_BITS;

        // Block has same starting address, we can just adjust the size.
        if (info.GetAddress() == addr) {
            // Block size matches, simply change attributes.
            if (info.GetSize() == num_pages << Memory::CITRA_PAGE_BITS) {
                begin->Initialize(addr, num_pages, tag, state, perms);
                R_SUCCEED();
            }

            // Block size is bigger, split, insert new block after and update
            begin->ShrinkBlock(addr, num_pages);
            KMemoryBlock* block = KMemoryBlock::Allocate(m_kernel);
            block->Initialize(addr, num_pages, tag, state, perms);

            // Insert it to the block list.
            m_blocks.insert(++begin, *block);
            R_SUCCEED();
        }

        // Same end address, but different base addr.
        if (info.GetLastAddress() == last_addr) {
            begin->ShrinkBlock(addr, num_pages);
            KMemoryBlock* block = KMemoryBlock::Allocate(m_kernel);
            block->Initialize(addr, num_pages, tag, state, perms);

            // Insert it to the block list
            m_blocks.insert(begin, *block);
            R_SUCCEED();
        }

        // Block fully contains start and end addresses. Shrink it to [last_addr, block_end] range.
        begin->ShrinkBlock(0, num_pages + (addr >> Memory::CITRA_PAGE_BITS));

        // Create a new block for [addr, last_addr] with the provided attributes.
        KMemoryBlock* middle_block = KMemoryBlock::Allocate(m_kernel);
        middle_block->Initialize(addr, num_pages, tag, state, perms);
        begin = m_blocks.insert(begin, *middle_block);

        // Create another block for the third range [block_addr, addr].
        KMemoryBlock* start_block = KMemoryBlock::Allocate(m_kernel);
        start_block->Initialize(info.GetAddress(), pages_in_block, 0, info.GetState(),
                                info.GetPerms());
        m_blocks.insert(begin, *start_block);
    }

    R_SUCCEED();
}

void KMemoryBlockManager::Finalize() {
    auto it = m_blocks.begin();
    while (it != m_blocks.end()) {
        KMemoryBlock::Free(m_kernel, std::addressof(*it));
        it = m_blocks.erase(it);
    }
}

} // namespace Kernel
