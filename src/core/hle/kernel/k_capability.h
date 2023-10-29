// SPDX-FileCopyrightText: Copyright 2024 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/bit_field.h"
#include "core/hle/result.h"

namespace Kernel {

class KPageTable;

class KCapability {
public:
    Result Initialize(std::span<const u32> caps, KPageTable& page_table);

    constexpr s32 GetHandleTableSize() const {
        return m_handle_table_size;
    }

private:
    enum class CapabilityType : u32 {
        InterruptInfo = 0b1110,
        SyscallMask = 0b11110,
        KernelVersion = 0b1111110,
        HandleTable = 0b11111110,
        KernelFlags = 0b111111110,
        MapIoRange = 0b11111111100,
        MapIoPage = 0b111111111110,
        Invalid = 0U,
        Padding = ~0U,
    };

    static constexpr CapabilityType GetCapabilityType(const u32 value) {
        return static_cast<CapabilityType>((~value & (value + 1)) - 1);
    }

    union SyscallMask {
        u32 raw;
        BitField<0, 24, u32> mask;
        BitField<24, 3, u32> index;
        BitField<27, 5, CapabilityType> type;
    };

    union KernelVersion {
        u32 raw;
        BitField<0, 8, u32> minor_version;
        BitField<8, 8, u32> major_version;
        BitField<25, 7, CapabilityType> type;
    };

    union KernelFlags {
        u32 raw;
        BitField<0, 1, u32> allow_debug;
        BitField<1, 1, u32> force_debug;
        BitField<2, 1, u32> allow_non_alphanum;
        BitField<3, 1, u32> shared_page_writing;
        BitField<4, 1, u32> priviledge_priority;
        BitField<5, 1, u32> allow_main_args;
        BitField<6, 1, u32> shared_device_memory;
        BitField<7, 1, u32> runnable_on_sleep;
        BitField<8, 4, u32> memory_type;
        BitField<12, 1, u32> special_memory;
        BitField<13, 1, u32> core2_access;
        BitField<21, 9, CapabilityType> type;
    };

    union HandleTable {
        u32 raw;
        BitField<0, 19, u32> size;
        BitField<24, 8, CapabilityType> type;
    };

    union MapIoPage {
        u32 raw;
        BitField<0, 20, u32> page_index;
        BitField<20, 1, u32> read_only;
        BitField<21, 11, CapabilityType> type;
    };

private:
    std::array<u32, 4> m_svc_acl{};
    std::array<u32, 4> m_irq_acl{};
    u32 m_kernel_flags;
    s16 m_handle_table_size;
    u16 m_intended_kernel_version;
};

} // namespace Kernel
