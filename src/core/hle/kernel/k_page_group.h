// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/k_linked_list.h"
#include "core/hle/kernel/slab_helpers.h"
#include "core/memory.h"

namespace Kernel {

struct KBlockInfo final : public KSlabAllocated<KBlockInfo> {
public:
    explicit KBlockInfo() = default;
    ~KBlockInfo() = default;

    void Initialize(u32 address, u32 num_pages) {
        m_base_address = address;
        m_num_pages = num_pages;
    }

    constexpr u32 GetAddress() const {
        return m_base_address;
    }

    constexpr u32 GetEndAddress() const {
        return this->GetAddress() + this->GetSize();
    }

    constexpr u32 GetSize() const {
        return m_num_pages << Memory::CITRA_PAGE_BITS;
    }

    constexpr u32 GetNumPages() const {
        return m_num_pages;
    }

    constexpr bool IsEquivalentTo(const KBlockInfo& rhs) const {
        return m_base_address == rhs.m_base_address && m_num_pages == rhs.m_num_pages;
    }

    constexpr bool operator==(const KBlockInfo& rhs) const {
        return this->IsEquivalentTo(rhs);
    }

    constexpr bool operator!=(const KBlockInfo& rhs) const {
        return !(*this == rhs);
    }

public:
    u32 m_base_address;
    u32 m_num_pages;
};

class KPageManager;
class KernelSystem;

class KPageGroup {
    using BlockInfoList = KLinkedList<KBlockInfo>;
    using iterator = BlockInfoList::const_iterator;

public:
    explicit KPageGroup(KernelSystem& kernel, KPageManager* page_manager)
        : m_kernel{kernel}, m_page_manager{page_manager}, m_blocks{kernel} {}
    ~KPageGroup();

    iterator begin() const {
        return this->m_blocks.begin();
    }
    iterator end() const {
        return this->m_blocks.end();
    }
    bool empty() const {
        return this->m_blocks.empty();
    }

    void AddRange(u32 addr, u32 num_pages);
    void IncrefPages();

    void EraseAll();
    void FreeMemory();

    u32 GetTotalNumPages();

    bool IsEquivalentTo(const KPageGroup& rhs) const;

    bool operator==(const KPageGroup& rhs) const {
        return this->IsEquivalentTo(rhs);
    }

    bool operator!=(const KPageGroup& rhs) const {
        return !(*this == rhs);
    }

private:
    KernelSystem& m_kernel;
    KPageManager* m_page_manager{};
    BlockInfoList m_blocks;
};

} // namespace Kernel
