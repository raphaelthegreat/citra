// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_page_heap.h"

namespace Kernel {

void KPageHeap::Initialize(VAddr region_start, u32 region_size) {
    m_region_start = region_start;
    m_region_size = region_size;

    // Insert a free block for the entire region.
    Block* first_block = m_memory.GetPointer<Block>(m_region_start);
    ASSERT(first_block);

    // Initialize the block.
    first_block->Initialize(region_start, this->GetNumPages());

    // Insert the block to our block list.
    m_blocks.push_front(*first_block);
}

u32 KPageHeap::GetTotalNumPages() {
    // Iterate over the blocks.
    u32 total_num_pages{};
    for (const auto& block : m_blocks) {
        total_num_pages += block.num_pages;
    }
    return total_num_pages;
}

void KPageHeap::FreeBlock(VAddr addr, u32 num_pages) {
    if (num_pages == 0) {
        return;
    }

    // Return if we can insert at the beginning.
    if (Insert(addr, num_pages, m_blocks.end(), m_blocks.begin())) {
        return;
    }

    // Find a suitable place to insert the new free block.
    for (auto it = m_blocks.begin(); it != m_blocks.end();) {
        if (Insert(addr, num_pages, it++, it++)) {
            break;
        }
    }
}

KPageHeap::BlockList KPageHeap::AllocateBackwards(u32 size) {
    if (size == 0) [[unlikely]] {
        return {};
    }

    // Initialize the list that will hold our blocks.
    u32 remaining = size;
    BlockList list;

    // Iterate over block list backwards.
    for (auto it = m_blocks.rbegin(); it != m_blocks.rend(); it++) {
        // If block does not cover remaining pages continue.
        const u32 num_pages = it->num_pages;
        if (remaining > num_pages) {
            remaining -= num_pages;
            continue;
        }

        // Split last block at our boundary.
        const u32 new_block_pages = num_pages - remaining;
        auto new_block = SplitBlock(it.base(), new_block_pages);
        ASSERT(new_block != m_blocks.end());

        // Return final block which points to our allocated memory.
        list.splice(list.begin(), m_blocks, new_block, m_blocks.rbegin().base());
        return list;
    }

    return {};
}

VAddr KPageHeap::AllocateContiguous(u32 size, u32 page_alignment) {
    if (m_blocks.empty() || size == 0) [[unlikely]] {
        return 0;
    }

    // Iterate free block list
    for (auto it = m_blocks.begin(); it != m_blocks.end(); it++) {
        // If the block is not page aligned, derive the number of pages to the next aligned address.
        u32 misalignment = 0;
        if (page_alignment > 1) {
            const u32 offset = (it->GetAddress() >> Memory::CITRA_PAGE_BITS) % page_alignment;
            if (offset) {
                misalignment = page_alignment - offset;
            }
        }

        // The block is too small to fit our range.
        if (size + misalignment > it->num_pages) {
            continue;
        }

        // Split the misaligned part of the block. The resuling block will be aligned.
        if (misalignment) {
            it = SplitBlock(it, misalignment);
        }

        // Shrink the block to the requested size and return it.
        SplitBlock(it, size);
        m_blocks.erase(it);
        return it->address;
    }

    return 0;
}

KPageHeap::iterator KPageHeap::SplitBlock(KPageHeap::iterator block, u32 block_new_size) {
    ASSERT(block->num_pages <= this->GetNumPages());
    if (!block_new_size || block->num_pages == block_new_size) [[unlikely]] {
        return m_blocks.end();
    }

    // Initialize the new block.
    const VAddr new_address = block->address + Memory::CITRA_PAGE_SIZE * block_new_size;
    Block* new_block = m_memory.GetPointer<Block>(new_address);
    new_block->Initialize(new_address, block->num_pages - block_new_size);

    // Adjust existing block size and insert the new block to the list.
    block->num_pages = block_new_size;
    m_blocks.insert(++block, *new_block);

    // Return an iterator the new block.
    return m_blocks.iterator_to(*new_block);
}

bool KPageHeap::Insert(VAddr freed_addr, u32 num_freed_pages, iterator prev, iterator next) {
    Block* block = m_memory.GetPointer<Block>(freed_addr);

    // Ensure the insertion area does not overlap with existing blocks.
    const VAddr freed_addr_end = freed_addr + (num_freed_pages << Memory::CITRA_PAGE_BITS);
    const VAddr right_addr =
        prev != m_blocks.end() ? prev->GetEndAddress() : this->GetRegionStart();
    const VAddr left_addr = next != m_blocks.end() ? next->GetAddress() : this->GetRegionEnd();
    if (right_addr > freed_addr || freed_addr_end > left_addr) {
        return false;
    }

    // Initialize and insert a new block covering the provided range.
    block->Initialize(freed_addr, num_freed_pages);
    const auto it = m_blocks.insert(next, *block);

    // If the previous and current blocks are contiguous, concatenate them.
    if (prev != m_blocks.end() && prev->GetEndAddress() == freed_addr) {
        m_blocks.erase(it);
        prev->num_pages += block->num_pages;
        block = std::addressof(*prev);
    }
    // If the current and next blocks are contiguous, concatenate them.
    if (next != m_blocks.end() && block->GetEndAddress() == next->GetAddress()) {
        m_blocks.erase(next);
        block->num_pages += next->num_pages;
    }

    return true;
}

} // namespace Kernel
