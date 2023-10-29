// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_memory_block.h"

namespace Kernel {

void KMemoryBlock::ShrinkBlock(VAddr addr, u32 num_pages) {
    const VAddr end_addr = addr + (num_pages << Memory::CITRA_PAGE_BITS) - 1;
    const VAddr last_addr = this->GetLastAddress();
    if (m_base_addr < end_addr && end_addr < last_addr) {
        m_base_addr = end_addr + 1;
        m_num_pages = (last_addr - end_addr) >> Memory::CITRA_PAGE_BITS;
        return;
    }
    if (m_base_addr < addr && addr < last_addr) {
        m_num_pages = (addr - m_base_addr) >> Memory::CITRA_PAGE_BITS;
        return;
    }
}

void KMemoryBlock::GrowBlock(VAddr addr, u32 num_pages) {
    const u32 end_addr = addr + (num_pages << Memory::CITRA_PAGE_BITS) - 1;
    const u32 last_addr = this->GetLastAddress();
    if (addr < m_base_addr) {
        m_base_addr = addr;
        m_num_pages = (last_addr - addr + 1) >> Memory::CITRA_PAGE_BITS;
    }
    if (last_addr < end_addr) {
        m_num_pages = (end_addr - m_base_addr + 1) >> Memory::CITRA_PAGE_BITS;
    }
}

bool KMemoryBlock::IncludesRange(VAddr addr, u32 num_pages) {
    const u32 end_addr = addr + (num_pages << Memory::CITRA_PAGE_BITS) - 1;
    const u32 last_addr = this->GetLastAddress();
    return m_base_addr >= addr && last_addr <= end_addr;
}

} // namespace Kernel
