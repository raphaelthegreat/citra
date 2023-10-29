// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "core/hle/kernel/slab_helpers.h"
#include "core/memory.h"

namespace Kernel {

enum class KMemoryPermission : u32 {
    None = 0x0,
    UserRead = 0x1,
    UserWrite = 0x2,
    UserReadWrite = UserRead | UserWrite,
    UserExecute = 0x4,
    UserReadExecute = UserRead | UserExecute,
    KernelRead = 0x8,
    KernelWrite = 0x10,
    KernelExecute = 0x20,
    KernelReadWrite = KernelRead | KernelWrite,
    DontCare = 0x10000000,
};
DECLARE_ENUM_FLAG_OPERATORS(KMemoryPermission)

enum class KMemoryState : u32 {
    Free = 0x0,
    Reserved = 0x1,
    Io = 0x2,
    Static = 0x3,
    Code = 0x4,
    Private = 0x5,
    Shared = 0x6,
    Continuous = 0x7,
    Aliased = 0x8,
    Alias = 0x9,
    Aliascode = 0xA,
    Locked = 0xB,
    KernelMask = 0xFF,

    FlagDeallocatable = 0x100,
    FlagProtectible = 0x200,
    FlagDebuggable = 0x400,
    FlagIpcAllowed = 0x800,
    FlagMapped = 0x1000,
    FlagPrivate = 0x2000,
    FlagShared = 0x4000,
    FlagsPrivateOrShared = 0x6000,
    FlagCodeAllowed = 0x8000,
    FlagsIpc = 0x1800,
    FlagsPrivateData = 0x3800,
    FlagsPrivateCodeAllowed = 0xB800,
    FlagsPrivateCode = 0xBC00,
    FlagsCode = 0x9C00,

    KernelIo = 0x1002,
    KernelStatic = 0x1003,
    KernelShared = 0x5806,
    KernelLinear = 0x3907,
    KernelAliased = 0x3A08,
    KernelAlias = 0x1A09,
    KernelAliasCode = 0x9C0A,
    PrivateAliasCode = 0xBC0A,
    PrivateCode = 0xBC04,
    PrivateData = 0xBB05,
    KernelLocked = 0x380B,
    FlagsAny = 0xFFFFFFFF,
};
DECLARE_ENUM_FLAG_OPERATORS(KMemoryState)

struct KMemoryInfo {
    VAddr m_base_address;
    u32 m_size;
    KMemoryPermission m_perms;
    KMemoryState m_state;

    constexpr VAddr GetAddress() const {
        return m_base_address;
    }

    constexpr u32 GetSize() const {
        return m_size;
    }

    constexpr u32 GetNumPages() const {
        return this->GetSize() >> Memory::CITRA_PAGE_BITS;
    }

    constexpr VAddr GetEndAddress() const {
        return this->GetAddress() + this->GetSize();
    }

    constexpr VAddr GetLastAddress() const {
        return this->GetEndAddress() - 1;
    }

    constexpr KMemoryPermission GetPerms() const {
        return m_perms;
    }

    constexpr KMemoryState GetState() const {
        return m_state;
    }
};

struct KMemoryBlock : public KSlabAllocated<KMemoryBlock> {
public:
    explicit KMemoryBlock() = default;

    constexpr void Initialize(VAddr base_addr, u32 num_pages, u32 tag, KMemoryState state,
                              KMemoryPermission perms) {
        m_base_addr = base_addr;
        m_num_pages = num_pages;
        m_permission = perms;
        m_memory_state = state;
        m_tag = tag;
    }

    constexpr bool Contains(VAddr addr) const {
        return this->GetAddress() <= addr && addr <= this->GetLastAddress();
    }

    constexpr KMemoryInfo GetInfo() const {
        return {
            .m_base_address = m_base_addr,
            .m_size = this->GetSize(),
            .m_perms = m_permission,
            .m_state = m_memory_state,
        };
    }

    constexpr bool HasProperties(KMemoryState s, KMemoryPermission p, u32 t) const {
        return m_memory_state == s && m_permission == p && m_tag == t;
    }

    constexpr bool HasSameProperties(const KMemoryBlock& rhs) const {
        return m_memory_state == rhs.m_memory_state && m_permission == rhs.m_permission &&
               m_tag == rhs.m_tag;
    }

    constexpr u32 GetSize() const {
        return m_num_pages << Memory::CITRA_PAGE_BITS;
    }

    constexpr u32 GetEndAddress() const {
        return this->GetAddress() + this->GetSize();
    }

    constexpr u32 GetLastAddress() const {
        return this->GetEndAddress() - 1;
    }

    constexpr u32 GetAddress() const {
        return m_base_addr;
    }

    constexpr u32 GetNumPages() const {
        return m_num_pages;
    }

    constexpr KMemoryPermission GetPermission() const {
        return m_permission;
    }

    constexpr KMemoryState GetState() const {
        return m_memory_state;
    }

    constexpr u32 GetTag() const {
        return m_tag;
    }

    void ShrinkBlock(VAddr addr, u32 num_pages);
    void GrowBlock(VAddr addr, u32 num_pages);
    bool IncludesRange(VAddr addr, u32 num_pages);

private:
    u32 m_base_addr{};
    u32 m_num_pages{};
    KMemoryPermission m_permission{};
    KMemoryState m_memory_state{};
    u32 m_tag{};
};

} // namespace Kernel
