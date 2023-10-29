// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/k_linked_list.h"
#include "core/hle/kernel/k_memory_block.h"

namespace Kernel {

class KMemoryBlockManager final {
    using BlockList = KLinkedList<KMemoryBlock>;
    using iterator = BlockList::iterator;
    using const_iterator = BlockList::const_iterator;

public:
    explicit KMemoryBlockManager(KernelSystem& kernel) : m_kernel{kernel}, m_blocks{kernel} {}
    ~KMemoryBlockManager() = default;

    void Initialize(u32 addr_space_start, u32 addr_sce_end);
    void Finalize();

    void CoalesceBlocks();
    s64 GetTotalCommittedMemory();
    Result Update(VAddr addr, u32 num_pages, KMemoryState state, KMemoryPermission perms, u32 tag);

    KMemoryBlock* FindFreeBlock(VAddr start, u32 num_pages, u32 block_num_pages);

    iterator FindIterator(VAddr address) {
        return std::find_if(m_blocks.begin(), m_blocks.end(),
                            [address](const auto& block) { return block.Contains(address); });
    }

    const KMemoryBlock* FindBlock(VAddr address) {
        if (const_iterator it = this->FindIterator(address); it != m_blocks.end()) {
            return std::addressof(*it);
        }

        return nullptr;
    }

private:
    KernelSystem& m_kernel;
    BlockList m_blocks;
};

} // namespace Kernel
