// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/page_table.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/k_memory_manager.h"
#include "core/hle/kernel/k_page_group.h"
#include "core/hle/kernel/k_page_manager.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/result.h"

namespace Kernel {

static constexpr u32 PageBits = Memory::CITRA_PAGE_BITS;

using TraversalEntry = Common::PageTable::TraversalEntry;
using TraversalContext = Common::PageTable::TraversalContext;

Result KPageTable::CheckAndUpdateAddrRangeMaskedStateAndPerms(
    u32 addr, u32 num_pages, KMemoryState state_mask, KMemoryState expected_state,
    KMemoryPermission min_perms, KMemoryState new_state, KMemoryPermission new_perms) {
    // Ensure the provided region has the expected state and permissions.
    R_TRY(this->CheckAddrRangeMaskedStateAndPerms(addr, num_pages << PageBits, state_mask,
                                                  expected_state, min_perms));

    // Change the memory state.
    R_RETURN(this->Operate(addr, num_pages, 0, new_state, new_perms,
                           KMemoryUpdateFlags::StateAndPerms, MemoryOperation::RegionBase));
}

Result KPageTable::CheckMemoryState(u32 addr, u32 size, KMemoryState state) {
    // Verify that we can query the address.
    KMemoryInfo info;
    u32 page_info;
    R_TRY(this->QueryInfo(std::addressof(info), std::addressof(page_info), addr));

    // Validate the states match expectation.
    R_UNLESS(info.GetState() == state, ResultInvalidAddressState);
    R_UNLESS(info.GetEndAddress() >= addr + size, ResultInvalidAddressState);
    R_SUCCEED();
}

Result KPageTable::CheckMemoryState(u32 addr, u32 size, KMemoryState state_mask,
                                    KMemoryState state) {
    // Verify that we can query the address.
    KMemoryInfo info;
    u32 page_info;
    R_TRY(this->QueryInfo(std::addressof(info), std::addressof(page_info), addr));

    // Validate the states match expectation.
    R_UNLESS((info.GetState() & state_mask) == state, ResultInvalidAddressState);
    R_UNLESS(info.GetEndAddress() >= addr + size, ResultInvalidAddressState);
    R_SUCCEED();
}

Result KPageTable::CheckAttributes(u32 addr, u32 size, KMemoryState state,
                                   KMemoryPermission perms) {
    // Verify that we can query the address.
    KMemoryInfo info;
    u32 page_info;
    R_TRY(this->QueryInfo(std::addressof(info), std::addressof(page_info), addr));

    // Validate the states match expectation.
    const VAddr end_addr = addr + size;
    R_UNLESS(info.GetState() == state, ResultInvalidAddressState);
    R_UNLESS((perms & ~info.GetPerms()) == KMemoryPermission::None, ResultInvalidAddressState);
    R_UNLESS(!size || addr <= end_addr - 1, ResultInvalidAddressState);
    R_UNLESS(end_addr <= info.GetEndAddress(), ResultInvalidAddressState);
    R_SUCCEED();
}

Result KPageTable::CheckAddrRangeMaskedStateAndPerms(u32 addr, u32 size, KMemoryState state_mask,
                                                     KMemoryState state,
                                                     KMemoryPermission min_perms) {
    // Validate provided address region.
    R_UNLESS(!size || addr <= addr + size - 1, ResultInvalidAddressState);

    while (true) {
        // Query the page table.
        KMemoryInfo info;
        u32 page_info;
        R_TRY(this->QueryInfo(std::addressof(info), std::addressof(page_info), addr));

        // Validate the states match expectation.
        R_UNLESS((info.GetState() & state_mask) == state, ResultInvalidAddressState);
        R_UNLESS((min_perms & ~info.GetPerms()) == KMemoryPermission::None,
                 ResultInvalidAddressState);

        // If we reached the end, we are done.
        R_SUCCEED_IF(info.GetEndAddress() >= addr + size);

        // Move to next block.
        addr = info.GetEndAddress();
        size -= info.GetEndAddress() - addr;
    }

    UNREACHABLE();
}

Result KPageTable::RemapMemoryInterprocess(KPageTable* src_page_table, u32 dstAddr, u32 srcAddr,
                                           u32 numPages, KMemoryState dstMemState,
                                           KMemoryPermission dstMemPerms) {
    // Create a page group from the source address space.
    KPageGroup group{m_kernel, m_page_manager};
    R_TRY(src_page_table->MakePageGroup(group, srcAddr, numPages));

    // Map to the destination address space.
    R_RETURN(this->OperateOnGroup(dstAddr, std::addressof(group), dstMemState, dstMemPerms,
                                  KMemoryUpdateFlags::None));
}

Result KPageTable::CheckAndUnmapPageGroup(VAddr addr, KPageGroup* pg) {
    // Make a new page group for the region.
    KPageGroup group{m_kernel, m_page_manager};
    R_TRY(this->MakePageGroup(group, addr, pg->GetTotalNumPages()));

    // Ensure the new group is equivalent to the provided one.
    R_UNLESS(pg->IsEquivalentTo(group), Result{0xE0A01835});

    // Unmap the pages in the group.
    R_RETURN(this->OperateOnGroup(addr, pg, KMemoryState::Free, KMemoryPermission::None,
                                  KMemoryUpdateFlags::None));
}

Result KPageTable::CreateAlias(VAddr src_addr, VAddr dst_addr, u32 num_pages,
                               KMemoryState expected_state_src,
                               KMemoryPermission expected_min_perms_src, KMemoryState new_state_src,
                               KMemoryPermission new_perms_src, KMemoryState new_state_dst,
                               KMemoryPermission new_perms_dst) {
    // Check the source memory block attributes match expected values.
    R_TRY(this->CheckAttributes(src_addr, num_pages << PageBits, expected_state_src,
                                expected_min_perms_src));

    // Check the destination memory block attributes match expected values.
    R_TRY(this->CheckAttributes(dst_addr, num_pages << PageBits, KMemoryState::Free,
                                KMemoryPermission::None));

    // Create a page group with the pages of the source range to alias.
    KPageGroup group{m_kernel, m_page_manager};
    R_TRY(this->MakePageGroup(group, src_addr, num_pages));

    // Update the source and destination region attributes.
    R_ASSERT(this->OperateOnGroup(src_addr, std::addressof(group), new_state_src, new_perms_src,
                                  KMemoryUpdateFlags::StateAndPerms));
    R_ASSERT(this->OperateOnGroup(dst_addr, std::addressof(group), new_state_dst, new_perms_dst,
                                  KMemoryUpdateFlags::None));
    R_SUCCEED();
}

Result KPageTable::DestroyAlias(VAddr src_addr, VAddr dst_addr, u32 num_pages,
                                KMemoryState expected_state_src,
                                KMemoryPermission expected_min_perms_src,
                                KMemoryState expected_state_dst,
                                KMemoryPermission expected_min_perms_dst,
                                KMemoryState new_state_src, KMemoryPermission new_perms_src) {
    // Check the source memory block attributes match expected values.
    R_TRY(this->CheckAttributes(src_addr, num_pages << PageBits, expected_state_src,
                                expected_min_perms_src));

    // Check the destination memory block attributes match expected values.
    R_TRY(this->CheckAttributes(dst_addr, num_pages << PageBits, expected_state_dst,
                                expected_min_perms_dst));

    // Create a page group with the pages of the destination range.
    KPageGroup dst_group{m_kernel, m_page_manager};
    R_TRY(this->MakePageGroup(dst_group, dst_addr, num_pages));

    // Create a page group with the pages of the source range.
    KPageGroup src_group{m_kernel, m_page_manager};
    R_TRY(this->MakePageGroup(src_group, src_addr, num_pages));

    // Ensure ranges are equivalent.
    R_UNLESS(src_group.IsEquivalentTo(dst_group), Result{0xD8E007F5});

    // Mark the aliased range as free.
    R_ASSERT(this->OperateOnGroup(dst_addr, std::addressof(dst_group), KMemoryState::Free,
                                  KMemoryPermission::None, KMemoryUpdateFlags::None));

    // Update state and permissions of the source range.
    R_ASSERT(this->OperateOnGroup(src_addr, std::addressof(src_group), new_state_src, new_perms_src,
                                  KMemoryUpdateFlags::StateAndPerms));

    R_SUCCEED();
}

void KPageTable::Unmap(VAddr addr, u32 num_pages) {
    // Create a page group with the pages to unmap.
    KPageGroup group{m_kernel, m_page_manager};
    this->MakePageGroup(group, addr, num_pages);

    // Unmap the memory region.
    m_memory.UnmapRegion(*m_impl, addr, num_pages << PageBits);

    // Iterate over the blocks and free them.
    for (const auto& block : group) {
        m_memory_manager->Free(block.GetAddress(), block.GetNumPages());
    }
}

Result KPageTable::OperateOnGroup(VAddr addr, KPageGroup* group, KMemoryState state,
                                  KMemoryPermission perms, KMemoryUpdateFlags update_flags) {
    // Ensure provided page group is not empty.
    R_THROW_IF(group->empty(), Result{0x82007FF});

    // Iterate the blocks and operate on them.
    for (auto& block : *group) {
        R_TRY(this->Operate(addr, block.GetNumPages(), block.GetAddress() - 0xC0000000, state,
                            perms, update_flags, MemoryOperation::RegionBase));
        addr += block.GetSize();
    }

    R_SUCCEED();
}

Result KPageTable::OperateOnAnyFreeBlockInRegionWithGuardPage(
    VAddr* out_addr, u32 block_num_pages, VAddr region_start, u32 region_num_pages, PAddr pa,
    KMemoryState state, KMemoryPermission perms, KMemoryUpdateFlags flags, MemoryOperation region) {

    // Setup tracking variables.
    const u32 block_size = block_num_pages << PageBits;
    VAddr block_addr = region_start;

    while (true) {
        KMemoryBlock* free_block =
            m_memory_block_manager.FindFreeBlock(region_start, region_num_pages, block_num_pages + 1);
        R_THROW_IF(!free_block, ResultOutOfMemory);

        const KMemoryInfo info = free_block->GetInfo();
        block_addr = std::max(info.GetAddress(), block_addr);

        if (info.GetLastAddress() > block_addr + block_size + Memory::CITRA_PAGE_SIZE) {
            const Result result = this->Operate(block_addr + Memory::CITRA_PAGE_SIZE,
                                                block_num_pages, pa, state, perms, flags, region);
            if (result.IsSuccess()) {
                *out_addr = block_addr + Memory::CITRA_PAGE_SIZE;
                R_SUCCEED();
            }

            R_ASSERT(result == ResultInvalidBlockState);
            block_addr = region_start;
        }
    }
    return ResultOutOfMemory;
}

Result KPageTable::Operate(VAddr address, u32 num_pages, PAddr pa, KMemoryState state,
                           KMemoryPermission perms, KMemoryUpdateFlags flags, MemoryOperation op) {
    // Updating permissions requires the memory to be already mapped.
    if (True(flags & KMemoryUpdateFlags::Perms) && False(flags & KMemoryUpdateFlags::State)) {
        KMemoryInfo info;
        u32 page_info;
        R_TRY(this->QueryInfo(std::addressof(info), std::addressof(page_info), address));
        R_UNLESS(True(info.m_state & KMemoryState::FlagMapped), Result{0xD8E007F5});
        state = info.m_state;
    }

    // After we are done, update the range in the memory block manager.
    ON_RESULT_SUCCESS {
        m_memory_block_manager.Update(address, num_pages, state, perms, 0);
    };

    // KLightScopedMutex m{m_mutex};
    const VAddr end_vaddr = address + (num_pages << PageBits);

    // Free the requested memory
    if (state == KMemoryState::Free) {
        this->Unmap(address, num_pages);
        R_SUCCEED();
    }

    // Ensure a block exists at that address, it contains our entire range and it's free.
    if (state == KMemoryState::Reserved) {
        const KMemoryBlock* block = m_memory_block_manager.FindBlock(address);
        const KMemoryInfo info = block->GetInfo();

        R_THROW_IF(info.GetState() != KMemoryState::Free, ResultInvalidAddressState);
        R_THROW_IF(info.GetEndAddress() < end_vaddr, ResultInvalidAddressState);
        R_SUCCEED();
    }

    if (False(flags & KMemoryUpdateFlags::State)) {
        const KMemoryBlock* block = m_memory_block_manager.FindBlock(address);
        R_THROW_IF(!block, ResultInvalidAddress);

        const KMemoryInfo info = block->GetInfo();
        state = info.GetState();

        R_THROW_IF(info.GetState() != KMemoryState::Free, ResultInvalidBlockState);
        R_THROW_IF(info.GetEndAddress() < end_vaddr, ResultInvalidBlockState);
    }

    R_SUCCEED_IF(True(flags & KMemoryUpdateFlags::StateAndPerms));

    // No physical address provided, the kernel will pick the next available one.
    if (!pa) {
        // Linear allocation can't be done by this function directly.
        R_THROW_IF(state == KMemoryState::KernelLinear, ResultInvalidCombinationKernel);

        // Allocate a chunk of physical memory for the region.
        const auto blocks = m_memory_manager->AllocateBackwards(num_pages, op);
        R_THROW_IF(blocks.empty(), ResultOutOfMemory);

        // Setup tracking variables.
        VAddr cur_addr = address;
        u32 remaining_pages = num_pages;

        for (auto it = blocks.begin(); it != blocks.end();) {
            // Retrieve information about the block and advance to the next one.
            const u32 num_pages = it->num_pages;
            const VAddr address = it->address;
            const u32 size = it->num_pages << PageBits;
            it++;

            // Initialize memory region of the block.
            m_memory.ZeroBlock(*m_impl, address, size);

            // Map the allocated block
            m_memory.MapMemoryRegion(*m_impl, cur_addr, num_pages << PageBits, address - 0xC0000000);

            cur_addr += size;
            remaining_pages -= num_pages;
        }

        R_SUCCEED_IF(!remaining_pages);
        UNREACHABLE();
    }

    // Map provided physical address to the virtual range.
    m_memory.MapMemoryRegion(*m_impl, address, num_pages << PageBits, pa);
    R_SUCCEED();
}

Result KPageTable::MakePageGroup(KPageGroup& pg, VAddr addr, u32 num_pages) {
    // KLightScopedMutex m{m_mutex};

    // Begin traversal.
    TraversalContext context;
    TraversalEntry next_entry;
    bool traverse_valid =
        m_impl->BeginTraversal(std::addressof(next_entry), std::addressof(context), addr);
    R_UNLESS(traverse_valid, ResultInvalidAddressState);

    // Prepare tracking variables.
    const size_t size = num_pages << PageBits;
    PAddr cur_addr = next_entry.phys_addr;
    size_t cur_size = next_entry.block_size - (cur_addr & (next_entry.block_size - 1));
    size_t tot_size = cur_size;

    const auto IsFcramPhysicalAddress = [](PAddr addr) {
        return addr >= Memory::FCRAM_PADDR && addr < Memory::FCRAM_N3DS_PADDR_END;
    };

    // Iterate, adding to group as we go.
    while (tot_size < size) {
        R_UNLESS(m_impl->ContinueTraversal(std::addressof(next_entry), std::addressof(context)),
                 ResultInvalidAddressState);

        if (next_entry.phys_addr != (cur_addr + cur_size)) {
            const size_t cur_pages = cur_size >> PageBits;

            R_UNLESS(IsFcramPhysicalAddress(cur_addr), ResultInvalidAddressState);
            pg.AddRange(cur_addr + 0xC0000000, cur_pages);

            cur_addr = next_entry.phys_addr;
            cur_size = next_entry.block_size;
        } else {
            cur_size += next_entry.block_size;
        }

        tot_size += next_entry.block_size;
    }

    // Ensure we add the right amount for the last block.
    if (tot_size > size) {
        cur_size -= (tot_size - size);
    }

    // Add the last block.
    const size_t cur_pages = cur_size / Memory::CITRA_PAGE_SIZE;
    R_UNLESS(IsFcramPhysicalAddress(cur_addr), ResultInvalidAddressState);
    pg.AddRange(cur_addr + 0xC0000000, cur_pages);

    R_SUCCEED();
}

Result KPageTable::QueryInfo(KMemoryInfo* out_info, u32* page_info, u32 addr) {
    // KLightScopedMutex m{m_mutex};

    // Find the block that contains the provided address.
    const KMemoryBlock* block = m_memory_block_manager.FindBlock(addr);
    R_UNLESS(out_info && block, ResultInvalidAddress);

    // Copy the block information to the output.
    const KMemoryInfo info = block->GetInfo();
    std::memcpy(out_info, &info, sizeof(KMemoryInfo));

    // We are finished.
    *page_info = 0;
    R_SUCCEED();
}

} // namespace Kernel
