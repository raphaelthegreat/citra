// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "core/hle/kernel/k_memory_block_manager.h"
#include "core/hle/result.h"

namespace Common {
struct PageTable;
}

namespace Kernel {

enum class KMemoryUpdateFlags {
    None = 0x0,
    State = 0x1,
    Perms = 0x100,
    StateAndPerms = State | Perms,
};
DECLARE_ENUM_FLAG_OPERATORS(KMemoryUpdateFlags)

enum class MemoryOperation : u32;

class KPageGroup;
class KPageManager;
class KMemoryManager;

class KPageTable {
public:
    explicit KPageTable(KernelSystem& kernel, KPageManager* page_manager)
        : m_kernel{kernel}, m_memory{kernel.memory}, m_page_manager{page_manager},
          m_memory_block_manager{kernel} {}
    ~KPageTable() = default;

    Common::PageTable& GetImpl() {
        return *m_impl;
    }

    Result CheckAndUpdateAddrRangeMaskedStateAndPerms(
        u32 addr, u32 num_pages, KMemoryState state_mask, KMemoryState expected_state,
        KMemoryPermission min_perms, KMemoryState new_state, KMemoryPermission new_perms);
    Result CheckMemoryState(u32 addr, u32 size, KMemoryState state);
    Result CheckMemoryState(u32 addr, u32 size, KMemoryState stateMask,
                            KMemoryState expectedStateFlags);
    Result CheckAttributes(u32 addr, u32 size, KMemoryState state, KMemoryPermission perms);
    Result CheckAddrRangeMaskedStateAndPerms(u32 addr, u32 size, KMemoryState stateMask,
                                             KMemoryState expectedState,
                                             KMemoryPermission minPerms);
    Result CheckAndChangeGroupStateAndPerms(u32 addr, KPageGroup* pgGroup, KMemoryState stateMask,
                                            KMemoryState expectedState, KMemoryPermission minPerms,
                                            KMemoryState newState, KMemoryPermission newPerms);

    Result RemapMemoryInterprocess(KPageTable* src_page_table, u32 dstAddr, u32 srcAddr,
                                   u32 numPages, KMemoryState dstMemState,
                                   KMemoryPermission dstMemPerms);

    Result CheckAndUnmapPageGroup(u32 addr, KPageGroup* pgGroup);

    Result CreateAlias(VAddr srcAddr, VAddr dstAddr, u32 numPages, KMemoryState expectedStateSrc,
                       KMemoryPermission expectedMinPermsSrc, KMemoryState newStateSrc,
                       KMemoryPermission newPermsSrc, KMemoryState newStateDst,
                       KMemoryPermission newPermsDst);
    Result DestroyAlias(VAddr srcAddr, VAddr dstAddr, u32 numPages, KMemoryState expectedStateSrc,
                        KMemoryPermission expectedMinPermsSrc, KMemoryState expectedStateDst,
                        KMemoryPermission expectedMinPermsDst, KMemoryState newStateSrc,
                        KMemoryPermission newPermsSrc);

    void Unmap(VAddr addr, u32 num_pages);

    Result OperateOnGroup(u32 addr, KPageGroup* pgGroup, KMemoryState state,
                          KMemoryPermission perms, KMemoryUpdateFlags updateFlags);
    Result OperateOnAnyFreeBlockInRegionWithGuardPage(VAddr* outAddr, u32 blockNumPages,
                                                      VAddr regionStart, u32 regionNumPages, PAddr pa,
                                                      KMemoryState state, KMemoryPermission perms,
                                                      KMemoryUpdateFlags updateFlags,
                                                      MemoryOperation region);
    Result Operate(u32 va, u32 numPages, u32 pa, KMemoryState state, KMemoryPermission perms,
                   KMemoryUpdateFlags updateFlags, MemoryOperation region);

    Result MakePageGroup(KPageGroup& pg, VAddr addr, u32 num_pages);
    Result QueryInfo(KMemoryInfo* outMemoryInfo, u32* pageInfo, u32 addr);

private:
    KernelSystem& m_kernel;
    Memory::MemorySystem& m_memory;
    KPageManager* m_page_manager;
    KMemoryManager* m_memory_manager;
    // KLightMutex mutex;
    std::unique_ptr<Common::PageTable> m_impl{};
    KMemoryBlockManager m_memory_block_manager;
    u32 m_address_space_start{};
    u32 m_address_space_end{};
    u32 m_linear_address_range_start{};
};

} // namespace Kernel
