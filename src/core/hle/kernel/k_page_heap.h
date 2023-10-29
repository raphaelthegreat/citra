// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/intrusive_list.h"
#include "core/memory.h"

namespace Kernel {

class KPageHeap final {
public:
    struct Block final : public Common::IntrusiveListBaseNode<Block> {
        u32 num_pages;
        VAddr address;
        u32 nonce;
        u32 mac;

        void Initialize(VAddr address, u32 num_pages, u32 nonce = 0, u32 mac = 0) {
            this->address = address;
            this->num_pages = num_pages;
            this->nonce = nonce;
            this->mac = mac;
        }

        VAddr GetAddress() const noexcept {
            return address;
        }

        VAddr GetEndAddress() const noexcept {
            return address + num_pages * Memory::CITRA_PAGE_BITS;
        }
    };
    using BlockList = Common::IntrusiveListBaseTraits<Block>::ListType;

    explicit KPageHeap(Memory::MemorySystem& memory) : m_memory{memory} {}
    ~KPageHeap() = default;

    constexpr u32 GetNumPages() const {
        return m_region_size >> Memory::CITRA_PAGE_BITS;
    }

    constexpr u32 GetRegionStart() const {
        return m_region_start;
    }

    constexpr u32 GetRegionEnd() const {
        return m_region_start + m_region_size;
    }

    constexpr bool Contains(u32 addr) const {
        return this->GetRegionStart() <= addr && addr < this->GetRegionEnd();
    }

public:
    void Initialize(VAddr region_start, u32 region_size);
    u32 GetTotalNumPages();

    BlockList AllocateBackwards(u32 size);
    VAddr AllocateContiguous(u32 size, u32 page_alignment);
    void FreeBlock(u32 addr, u32 num_pages);

private:
    using iterator = BlockList::iterator;

    iterator SplitBlock(iterator block, u32 new_block_size);
    bool Insert(VAddr freed_addr, u32 num_freed_pages, iterator prev, iterator next);

private:
    BlockList m_blocks{};
    Memory::MemorySystem& m_memory;
    u32 m_region_start{};
    u32 m_region_size{};
    std::array<u32, 4> m_key{};
};

} // namespace Kernel
