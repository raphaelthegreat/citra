// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Memory {
class MemorySystem;
}

namespace Kernel {

enum class MemoryOperation : u32 {
    None = 0x0,
    RegionApplication = 0x100,
    RegionSystem = 0x200,
    RegionBase = 0x300,
    Kernel = 0x80000000,
    RegionBaseKernel = Kernel | RegionBase,
    Free = 0x1,
    Reserve = 0x2,
    Alloc = 0x3,
    Map = 0x4,
    Unmap = 0x5,
    Prot = 0x6,
    OpMask = 0xFF,
    RegionMask = 0xF00,
    LinearFlag = 0x10000,
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryOperation)

class KMemoryManager;

class KPageManager {
public:
    explicit KPageManager(Memory::MemorySystem& memory, KMemoryManager* memory_manager)
        : m_memory{memory}, m_memory_manager{memory_manager} {}
    ~KPageManager() = default;

    std::atomic<u32>& GetKernelMemoryUsage() noexcept {
        return m_kernel_memory_usage;
    }

    void Initialize(VAddr start_addr, u32 num_pages);
    void Open(VAddr addr, u32 num_pages);
    void Close(VAddr data, u32 num_pages, MemoryOperation op);

private:
    Memory::MemorySystem& m_memory;
    KMemoryManager* m_memory_manager{};
    VAddr m_start_addr{};
    u32 m_num_pages{};
    u32* m_page_ref_counts{};
    std::atomic<u32> m_kernel_memory_usage{};
    // KLightMutex m_mutex;
};

} // namespace Kernel
