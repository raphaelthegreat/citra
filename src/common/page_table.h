// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Common {

enum class PageType : u8 {
    /// Page is unmapped and should cause an access error.
    Unmapped,
    /// Page is mapped to regular memory. This is the only type you can get pointers to.
    Memory,
    /// Page is mapped to regular memory, but also needs to check for rasterizer cache flushing and
    /// invalidation
    RasterizerCachedMemory,
};

/**
 * A (reasonably) fast way of allowing switchable and remappable process address spaces. It loosely
 * mimics the way a real CPU page table works.
 */
struct PageTable {
    struct TraversalEntry {
        u64 phys_addr{};
        std::size_t block_size{};
    };

    struct TraversalContext {
        u64 next_page{};
        u64 next_offset{};
    };

    /// Number of bits reserved for attribute tagging.
    /// This can be at most the guaranteed alignment of the pointers in the page table.
    static constexpr int ATTRIBUTE_BITS = 2;
    static constexpr size_t PAGE_BITS = 12;
    static constexpr size_t PAGE_SIZE = 1ULL << PAGE_BITS;
    static constexpr size_t NUM_ENTRIES = 1 << (32 - PAGE_BITS);

    PageTable();
    ~PageTable() noexcept;

    PageTable(const PageTable&) = delete;
    PageTable& operator=(const PageTable&) = delete;

    PageTable(PageTable&&) noexcept = default;
    PageTable& operator=(PageTable&&) noexcept = default;

    bool BeginTraversal(TraversalEntry* out_entry, TraversalContext* out_context,
                        VAddr address) const;
    bool ContinueTraversal(TraversalEntry* out_entry, TraversalContext* context) const;

    PAddr GetPhysicalAddress(VAddr virt_addr) const {
        return backing_addr[virt_addr >> PAGE_BITS];
    }

    /**
     * Vector of memory pointers backing each page. An entry can only be non-null if the
     * corresponding attribute element is of type `Memory`.
     */
    std::array<u8*, NUM_ENTRIES> pointers;
    std::array<PageType, NUM_ENTRIES> attributes;
    std::array<PAddr, NUM_ENTRIES> backing_addr;
};

} // namespace Common
