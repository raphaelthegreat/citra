// SPDX-FileCopyrightText: Copyright 2024 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_capability.h"

namespace Kernel {

Result KCapability::Initialize(std::span<const u32> caps, KPageTable& page_table) {
    for (size_t i = 0; i < caps.size(); i++) {
        const u32 cap{caps[i]};

        if (GetCapabilityType(cap) == CapabilityType::MapRange) {
            // Check that the pair cap exists.
            R_UNLESS((++i) < caps.size(), Result{0xD9001412});

            // Check the pair cap is a map range cap.
            const u32 size_cap{caps[i]};
            R_UNLESS(GetCapabilityType(size_cap) == CapabilityType::MapRange,
                     Result{0xD9001412});

            // Map the range.
            R_TRY(this->MapRange_(cap, size_cap, page_table));
        } else {
            R_TRY(this->SetCapability(cap, set_flags, set_svc, page_table));
        }
    }

    R_SUCCEED();
}

} // namespace Kernel
