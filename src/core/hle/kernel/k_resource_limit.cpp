// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "common/settings.h"
#include "core/hle/kernel/k_auto_object_container.h"
#include "core/hle/kernel/k_resource_limit.h"

SERIALIZE_EXPORT_IMPL(Kernel::KResourceLimit)
SERIALIZE_EXPORT_IMPL(Kernel::ResourceLimitList)

namespace Kernel {

KResourceLimit::KResourceLimit(KernelSystem& kernel)
    : KAutoObjectWithSlabHeapAndContainer(kernel) {}

KResourceLimit::~KResourceLimit() = default;

s32 KResourceLimit::GetCurrentValue(ResourceLimitType type) const {
    const auto index = static_cast<size_t>(type);
    return m_current_values[index];
}

s32 KResourceLimit::GetLimitValue(ResourceLimitType type) const {
    const auto index = static_cast<size_t>(type);
    return m_limit_values[index];
}

void KResourceLimit::SetLimitValue(ResourceLimitType type, s32 value) {
    const auto index = static_cast<size_t>(type);
    m_limit_values[index] = value;
}

bool KResourceLimit::Reserve(ResourceLimitType type, s32 amount) {
    const auto index = static_cast<size_t>(type);
    const s32 limit = m_limit_values[index];
    const s32 new_value = m_current_values[index] + amount;
    if (new_value > limit) {
        LOG_ERROR(Kernel, "New value {} exceeds limit {} for resource type {}", new_value, limit,
                  type);
        return false;
    }
    m_current_values[index] = new_value;
    return true;
}

bool KResourceLimit::Release(ResourceLimitType type, s32 amount) {
    const auto index = static_cast<size_t>(type);
    const s32 value = m_current_values[index];
    if (amount > value) {
        LOG_ERROR(Kernel, "Amount {} exceeds current value {} for resource type {}", amount, value,
                  type);
        return false;
    }
    m_current_values[index] = value - amount;
    return true;
}

template <class Archive>
void KResourceLimit::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<KAutoObject>(*this);
    ar& m_name;
    ar& m_limit_values;
    ar& m_current_values;
}
SERIALIZE_IMPL(KResourceLimit)

ResourceLimitList::ResourceLimitList(KernelSystem& kernel) {
    // PM makes APPMEMALLOC always match app RESLIMIT_COMMIT.
    // See: https://github.com/LumaTeam/Luma3DS/blob/e2778a45/sysmodules/pm/source/reslimit.c#L275
    const bool is_new_3ds = Settings::values.is_new_3ds.GetValue();
    const auto& appmemalloc = kernel.GetMemoryRegion(MemoryRegion::APPLICATION);

    const auto CreateLimit = [&](std::string name) {
        KResourceLimit* limit = KResourceLimit::Create(kernel);
        limit->Initialize(name);
        KResourceLimit::Register(kernel, limit);
        return limit;
    };

    // Create the Application resource limit
    auto resource_limit = CreateLimit("Applications");
    resource_limit->SetLimitValue(ResourceLimitType::Priority, 0x18);
    resource_limit->SetLimitValue(ResourceLimitType::Commit, appmemalloc->size);
    resource_limit->SetLimitValue(ResourceLimitType::Thread, 0x20);
    resource_limit->SetLimitValue(ResourceLimitType::Event, 0x20);
    resource_limit->SetLimitValue(ResourceLimitType::Mutex, 0x20);
    resource_limit->SetLimitValue(ResourceLimitType::Semaphore, 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::Timer, 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::SharedMemory, 0x10);
    resource_limit->SetLimitValue(ResourceLimitType::AddressArbiter, 0x2);
    resource_limit->SetLimitValue(ResourceLimitType::CpuTime, 0x0);
    resource_limits[static_cast<u8>(ResourceLimitCategory::Application)] = resource_limit;

    // Create the SysApplet resource limit
    resource_limit = CreateLimit("System Applets");
    resource_limit->SetLimitValue(ResourceLimitType::Priority, 0x4);
    resource_limit->SetLimitValue(ResourceLimitType::Commit, is_new_3ds ? 0x5E06000 : 0x2606000);
    resource_limit->SetLimitValue(ResourceLimitType::Thread, is_new_3ds ? 0x1D : 0xE);
    resource_limit->SetLimitValue(ResourceLimitType::Event, is_new_3ds ? 0xB : 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::Mutex, 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::Semaphore, 0x4);
    resource_limit->SetLimitValue(ResourceLimitType::Timer, 0x4);
    resource_limit->SetLimitValue(ResourceLimitType::SharedMemory, 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::AddressArbiter, 0x3);
    resource_limit->SetLimitValue(ResourceLimitType::CpuTime, 0x2710);
    resource_limits[static_cast<u8>(ResourceLimitCategory::SysApplet)] = resource_limit;

    // Create the LibApplet resource limit
    resource_limit = CreateLimit("Library Applets");
    resource_limit->SetLimitValue(ResourceLimitType::Priority, 0x4);
    resource_limit->SetLimitValue(ResourceLimitType::Commit, 0x602000);
    resource_limit->SetLimitValue(ResourceLimitType::Thread, 0xE);
    resource_limit->SetLimitValue(ResourceLimitType::Event, 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::Mutex, 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::Semaphore, 0x4);
    resource_limit->SetLimitValue(ResourceLimitType::Timer, 0x4);
    resource_limit->SetLimitValue(ResourceLimitType::SharedMemory, 0x8);
    resource_limit->SetLimitValue(ResourceLimitType::AddressArbiter, 0x1);
    resource_limit->SetLimitValue(ResourceLimitType::CpuTime, 0x2710);
    resource_limits[static_cast<u8>(ResourceLimitCategory::LibApplet)] = resource_limit;

    // Create the Other resource limit
    resource_limit = CreateLimit("Others");
    resource_limit->SetLimitValue(ResourceLimitType::Priority, 0x4);
    resource_limit->SetLimitValue(ResourceLimitType::Commit, is_new_3ds ? 0x2182000 : 0x1682000);
    resource_limit->SetLimitValue(ResourceLimitType::Thread, is_new_3ds ? 0xE1 : 0xCA);
    resource_limit->SetLimitValue(ResourceLimitType::Event, is_new_3ds ? 0x108 : 0xF8);
    resource_limit->SetLimitValue(ResourceLimitType::Mutex, is_new_3ds ? 0x25 : 0x23);
    resource_limit->SetLimitValue(ResourceLimitType::Semaphore, is_new_3ds ? 0x43 : 0x40);
    resource_limit->SetLimitValue(ResourceLimitType::Timer, is_new_3ds ? 0x2C : 0x2B);
    resource_limit->SetLimitValue(ResourceLimitType::SharedMemory, is_new_3ds ? 0x1F : 0x1E);
    resource_limit->SetLimitValue(ResourceLimitType::AddressArbiter, is_new_3ds ? 0x2D : 0x2B);
    resource_limit->SetLimitValue(ResourceLimitType::CpuTime, 0x3E8);
    resource_limits[static_cast<u8>(ResourceLimitCategory::Other)] = resource_limit;
}

ResourceLimitList::~ResourceLimitList() = default;

KResourceLimit* ResourceLimitList::GetForCategory(ResourceLimitCategory category) {
    switch (category) {
    case ResourceLimitCategory::Application:
    case ResourceLimitCategory::SysApplet:
    case ResourceLimitCategory::LibApplet:
    case ResourceLimitCategory::Other:
        return resource_limits[static_cast<u8>(category)];
    default:
        UNREACHABLE_MSG("Unknown resource limit category");
    }
}

template <class Archive>
void ResourceLimitList::serialize(Archive& ar, const unsigned int) {
    ar& resource_limits;
}
SERIALIZE_IMPL(ResourceLimitList)

} // namespace Kernel
