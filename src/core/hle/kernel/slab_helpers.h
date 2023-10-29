// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

template <class Derived>
class KSlabAllocated {
public:
    constexpr KSlabAllocated() = default;

    size_t GetSlabIndex(KernelSystem& kernel) const {
        return kernel.SlabHeap<Derived>().GetIndex(static_cast<const Derived*>(this));
    }

public:
    static void InitializeSlabHeap(KernelSystem& kernel, void* memory, size_t memory_size) {
        kernel.SlabHeap<Derived>().Initialize(memory, memory_size);
    }

    static Derived* Allocate(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().Allocate(kernel);
    }

    static void Free(KernelSystem& kernel, Derived* obj) {
        kernel.SlabHeap<Derived>().Free(obj);
    }

    static size_t GetObjectSize(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetObjectSize();
    }

    static size_t GetSlabHeapSize(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetSlabHeapSize();
    }

    static size_t GetPeakIndex(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetPeakIndex();
    }

    static uintptr_t GetSlabHeapAddress(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetSlabHeapAddress();
    }

    static size_t GetNumRemaining(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetNumRemaining();
    }
};

template <typename Derived, typename Base>
class KAutoObjectWithSlabHeap : public Base {
    static_assert(std::is_base_of<KAutoObject, Base>::value);

private:
    static Derived* Allocate(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().Allocate(kernel);
    }

    static void Free(KernelSystem& kernel, Derived* obj) {
        kernel.SlabHeap<Derived>().Free(obj);
    }

public:
    explicit KAutoObjectWithSlabHeap(KernelSystem& kernel) : Base(kernel) {}
    virtual ~KAutoObjectWithSlabHeap() = default;

    virtual void Destroy() override {
        const bool is_initialized = this->IsInitialized();
        uintptr_t arg = 0;
        if (is_initialized) {
            arg = this->GetPostDestroyArgument();
            this->Finalize();
        }
        Free(Base::m_kernel, static_cast<Derived*>(this));
        if (is_initialized) {
            Derived::PostDestroy(arg);
        }
    }

    virtual bool IsInitialized() const {
        return true;
    }
    virtual uintptr_t GetPostDestroyArgument() const {
        return 0;
    }

    size_t GetSlabIndex() const {
        return SlabHeap<Derived>(Base::m_kernel).GetObjectIndex(static_cast<const Derived*>(this));
    }

public:
    static void InitializeSlabHeap(KernelSystem& kernel, void* memory, size_t memory_size) {
        kernel.SlabHeap<Derived>().Initialize(memory, memory_size);
    }

    static Derived* Create(KernelSystem& kernel) {
        Derived* obj = Allocate(kernel);
        if (obj != nullptr) {
            KAutoObject::Create(obj);
        }
        return obj;
    }

    static size_t GetObjectSize(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetObjectSize();
    }

    static size_t GetSlabHeapSize(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetSlabHeapSize();
    }

    static size_t GetPeakIndex(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetPeakIndex();
    }

    static uintptr_t GetSlabHeapAddress(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetSlabHeapAddress();
    }

    static size_t GetNumRemaining(KernelSystem& kernel) {
        return kernel.SlabHeap<Derived>().GetNumRemaining();
    }
};

} // namespace Kernel
