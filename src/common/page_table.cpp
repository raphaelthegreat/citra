// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/page_table.h"

namespace Common {

PageTable::PageTable() = default;

PageTable::~PageTable() noexcept = default;

bool PageTable::BeginTraversal(TraversalEntry* out_entry, TraversalContext* out_context,
                               VAddr address) const {
    // Setup invalid defaults.
    out_entry->phys_addr = 0;
    out_entry->block_size = PAGE_SIZE;
    out_context->next_page = 0;

    // Validate that we can read the actual entry.
    const auto page = address >> PAGE_BITS;
    if (page >= backing_addr.size()) {
        return false;
    }

    // Validate that the entry is mapped.
    const auto phys_addr = backing_addr[page];
    if (phys_addr == 0) {
        return false;
    }

    // Populate the results.
    out_entry->phys_addr = phys_addr;
    out_context->next_page = page + 1;
    out_context->next_offset = address + PAGE_SIZE;
    return true;
}

bool PageTable::ContinueTraversal(TraversalEntry* out_entry, TraversalContext* context) const {
    // Setup invalid defaults.
    out_entry->phys_addr = 0;
    out_entry->block_size = PAGE_SIZE;

    // Validate that we can read the actual entry.
    const auto page = context->next_page;
    if (page >= backing_addr.size()) {
        return false;
    }

    // Validate that the entry is mapped.
    const auto phys_addr = backing_addr[page];
    if (phys_addr == 0) {
        return false;
    }

    // Populate the results.
    out_entry->phys_addr = phys_addr;
    context->next_page = page + 1;
    context->next_offset += PAGE_SIZE;
    return true;
}

} // namespace Common
