// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_CHECKED_PTR_H_
#define BASE_MEMORY_CHECKED_PTR_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#define ENABLE_BACKUP_REF_PTR_IMPL 0
#if ENABLE_BACKUP_REF_PTR_IMPL
static_assert(ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR,
              "BackupRefPtrImpl can only by used if PartitionRefCount is "
              "enabled");
#endif

namespace base {

// NOTE: All methods should be ALWAYS_INLINE. CheckedPtr is meant to be a
// lightweight replacement of a raw pointer, hence performance is critical.

namespace internal {
// These classes/structures are part of the CheckedPtr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

struct CheckedPtrNoOpImpl {
  // Wraps a pointer, and returns its uintptr_t representation.
  // Use |const volatile| to prevent compiler error. These will be dropped
  // anyway when casting to uintptr_t and brought back upon pointer extraction.
  static ALWAYS_INLINE uintptr_t WrapRawPtr(const volatile void* cv_ptr) {
    return reinterpret_cast<uintptr_t>(cv_ptr);
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  static ALWAYS_INLINE void ReleaseWrappedPtr(uintptr_t) {}

  // Returns equivalent of |WrapRawPtr(nullptr)|. Separated out to make it a
  // constexpr.
  static constexpr ALWAYS_INLINE uintptr_t GetWrappedNullPtr() {
    // This relies on nullptr and 0 being equal in the eyes of reinterpret_cast,
    // which apparently isn't true in all environments.
    return 0;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr uintptr_t Upcast(uintptr_t wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    return reinterpret_cast<uintptr_t>(
        static_cast<To*>(reinterpret_cast<From*>(wrapped_ptr)));
  }

  // Advance the wrapped pointer by |delta| bytes.
  static ALWAYS_INLINE uintptr_t Advance(uintptr_t wrapped_ptr, size_t delta) {
    return wrapped_ptr + delta;
  }

  // Returns a copy of a wrapped pointer, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE uintptr_t Duplicate(uintptr_t wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
};

#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)

#if ENABLE_BACKUP_REF_PTR_IMPL

struct BackupRefPtrImpl {
  // Note that `BackupRefPtrImpl` itself is not thread-safe. If multiple threads
  // modify the same smart pointer object without synchronization, a data race
  // will occur.

  // Wraps a pointer, and returns its uintptr_t representation.
  // Use |const volatile| to prevent compiler error. These will be dropped
  // anyway when casting to uintptr_t and brought back upon pointer extraction.
  static ALWAYS_INLINE uintptr_t WrapRawPtr(const volatile void* cv_ptr) {
    void* ptr = const_cast<void*>(cv_ptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    if (LIKELY(IsManagedByPartitionAllocNormalBuckets(ptr)))
      PartitionRefCountPointer(ptr)->AddRef();

    return addr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  static ALWAYS_INLINE void ReleaseWrappedPtr(uintptr_t wrapped_ptr) {
    void* ptr = reinterpret_cast<void*>(wrapped_ptr);

    // This check already covers the nullptr case.
    if (LIKELY(IsManagedByPartitionAllocNormalBuckets(ptr)))
      PartitionRefCountPointer(ptr)->Release();
  }

  // Returns equivalent of |WrapRawPtr(nullptr)|. Separated out to make it a
  // constexpr.
  static constexpr ALWAYS_INLINE uintptr_t GetWrappedNullPtr() {
    // This relies on nullptr and 0 being equal in the eyes of reinterpret_cast,
    // which apparently isn't true in all environments.
    return 0;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr uintptr_t Upcast(uintptr_t wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    return reinterpret_cast<uintptr_t>(
        static_cast<To*>(reinterpret_cast<From*>(wrapped_ptr)));
  }

  // Advance the wrapped pointer by |delta| bytes.
  static ALWAYS_INLINE uintptr_t Advance(uintptr_t wrapped_ptr, size_t delta) {
    return wrapped_ptr + delta;
  }

  // Returns a copy of a wrapped pointer, without making an assertion
  // on whether memory was freed or not. This method increments the reference
  // count of the allocation slot.
  static ALWAYS_INLINE uintptr_t Duplicate(uintptr_t wrapped_ptr) {
    return WrapRawPtr(reinterpret_cast<void*>(wrapped_ptr));
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
};

#endif  // ENABLE_BACKUP_REF_PTR_IMPL

#endif  // defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)

}  // namespace internal

// DO NOT USE! EXPERIMENTAL ONLY! This is helpful for local testing!
//
// CheckedPtr is meant to be a pointer wrapper, that will crash on
// Use-After-Free (UaF) to prevent security issues. This is very much in the
// experimental phase. More context in:
// https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg
//
// For now, CheckedPtr is a no-op wrapper to aid local testing.
//
// Goals for this API:
// 1. Minimize amount of caller-side changes as much as physically possible.
// 2. Keep this class as small as possible, while still satisfying goal #1 (i.e.
//    we aren't striving to maximize compatibility with raw pointers, merely
//    adding support for cases encountered so far).
template <typename T,
#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL) && \
    BUILDFLAG(USE_PARTITION_ALLOC)
#if ENABLE_BACKUP_REF_PTR_IMPL
          typename Impl = internal::BackupRefPtrImpl>
#else
          typename Impl = internal::CheckedPtrNoOpImpl>
#endif
#else  // defined(ARCH_CPU_64_BITS) && !defined(OS_NACL) &&
       // BUILDFLAG(USE_PARTITION_ALLOC)
          typename Impl = internal::CheckedPtrNoOpImpl>
#endif
class CheckedPtr {
 public:
#if ENABLE_BACKUP_REF_PTR_IMPL

  // BackupRefPtr requires a non-trivial default constructor, destructor, etc.
  constexpr ALWAYS_INLINE CheckedPtr() noexcept
      : wrapped_ptr_(Impl::GetWrappedNullPtr()) {}

  CheckedPtr(const CheckedPtr& p) noexcept
      : wrapped_ptr_(Impl::Duplicate(p.wrapped_ptr_)) {}

  CheckedPtr(CheckedPtr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    p.wrapped_ptr_ = Impl::GetWrappedNullPtr();
  }

  CheckedPtr& operator=(const CheckedPtr& p) {
    // Duplicate before releasing, in case the pointer is assigned to itself.
    uintptr_t new_ptr = Impl::Duplicate(p.wrapped_ptr_);
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = new_ptr;
    return *this;
  }

  CheckedPtr& operator=(CheckedPtr&& p) {
    if (LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(wrapped_ptr_);
      wrapped_ptr_ = p.wrapped_ptr_;
      p.wrapped_ptr_ = Impl::GetWrappedNullPtr();
    }
    return *this;
  }

  ALWAYS_INLINE ~CheckedPtr() noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    // Work around external issues where CheckedPtr is used after destruction.
    wrapped_ptr_ = Impl::GetWrappedNullPtr();
  }

#else  // ENABLE_BACKUP_REF_PTR_IMPL

  // CheckedPtr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).  This is needed for compatibility with raw pointers.
  //
  // TODO(lukasza): Always initialize |wrapped_ptr_|.  Fix resulting build
  // errors.  Analyze performance impact.
  constexpr CheckedPtr() noexcept = default;

  // In addition to nullptr_t ctor above, CheckedPtr needs to have these
  // as |=default| or |constexpr| to avoid hitting -Wglobal-constructors in
  // cases like this:
  //     struct SomeStruct { int int_field; CheckedPtr<int> ptr_field; };
  //     SomeStruct g_global_var = { 123, nullptr };
  CheckedPtr(const CheckedPtr&) noexcept = default;
  CheckedPtr(CheckedPtr&&) noexcept = default;
  CheckedPtr& operator=(const CheckedPtr&) noexcept = default;
  CheckedPtr& operator=(CheckedPtr&&) noexcept = default;

  ~CheckedPtr() = default;

#endif  // ENABLE_BACKUP_REF_PTR_IMPL

  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr ALWAYS_INLINE CheckedPtr(std::nullptr_t) noexcept
      : wrapped_ptr_(Impl::GetWrappedNullPtr()) {}

  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  ALWAYS_INLINE CheckedPtr(T* p) noexcept : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE CheckedPtr(const CheckedPtr<U, Impl>& ptr) noexcept
      : wrapped_ptr_(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE CheckedPtr(CheckedPtr<U, Impl>&& ptr) noexcept
      : wrapped_ptr_(Impl::template Upcast<T, U>(ptr.wrapped_ptr_)) {
#if ENABLE_BACKUP_REF_PTR_IMPL
    ptr.wrapped_ptr_ = Impl::GetWrappedNullPtr();
#endif
  }

  ALWAYS_INLINE CheckedPtr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::GetWrappedNullPtr();
    return *this;
  }
  ALWAYS_INLINE CheckedPtr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE CheckedPtr& operator=(const CheckedPtr<U, Impl>& ptr) noexcept {
    DCHECK(reinterpret_cast<uintptr_t>(this) !=
           reinterpret_cast<uintptr_t>(&ptr));
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ =
        Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_));
    return *this;
  }
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE CheckedPtr& operator=(CheckedPtr<U, Impl>&& ptr) noexcept {
    DCHECK(reinterpret_cast<uintptr_t>(this) !=
           reinterpret_cast<uintptr_t>(&ptr));
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::template Upcast<T, U>(ptr.wrapped_ptr_);
#if ENABLE_BACKUP_REF_PTR_IMPL
    ptr.wrapped_ptr_ = Impl::GetWrappedNullPtr();
#endif
    return *this;
  }

  // Avoid using. The goal of CheckedPtr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  ALWAYS_INLINE T* get() const { return GetForExtraction(); }

  explicit ALWAYS_INLINE operator bool() const {
    return wrapped_ptr_ != Impl::GetWrappedNullPtr();
  }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void<typename std::remove_cv<U>::type>::value>>
  ALWAYS_INLINE U& operator*() const {
    return *GetForDereference();
  }
  ALWAYS_INLINE T* operator->() const { return GetForDereference(); }
  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  ALWAYS_INLINE operator T*() const { return GetForExtraction(); }
  template <typename U>
  explicit ALWAYS_INLINE operator U*() const {
    return static_cast<U*>(GetForExtraction());
  }

  ALWAYS_INLINE CheckedPtr& operator++() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, sizeof(T));
    return *this;
  }
  ALWAYS_INLINE CheckedPtr& operator--() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, -sizeof(T));
    return *this;
  }
  ALWAYS_INLINE CheckedPtr operator++(int /* post_increment */) {
    CheckedPtr result = *this;
    ++(*this);
    return result;
  }
  ALWAYS_INLINE CheckedPtr operator--(int /* post_decrement */) {
    CheckedPtr result = *this;
    --(*this);
    return result;
  }
  ALWAYS_INLINE CheckedPtr& operator+=(ptrdiff_t delta_elems) {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems * sizeof(T));
    return *this;
  }
  ALWAYS_INLINE CheckedPtr& operator-=(ptrdiff_t delta_elems) {
    return *this += -delta_elems;
  }

  // Be careful to cover all cases with CheckedPtr being on both sides, left
  // side only and right side only. If any case is missed, a more costly
  // |operator T*()| will get called, instead of |operator==|.
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs,
                                       const CheckedPtr& rhs) {
    return lhs.GetForComparison() == rhs.GetForComparison();
  }
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs,
                                       const CheckedPtr& rhs) {
    return !(lhs == rhs);
  }
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs, T* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs, T* rhs) {
    return !(lhs == rhs);
  }
  friend ALWAYS_INLINE bool operator==(T* lhs, const CheckedPtr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  friend ALWAYS_INLINE bool operator!=(T* lhs, const CheckedPtr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  // Needed for cases like |derived_ptr == base_ptr|. Without these, a more
  // costly |operator T*()| will get called, instead of |operator==|.
  template <typename U>
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs,
                                       const CheckedPtr<U, Impl>& rhs) {
    // Add |const volatile| when casting, in case |U| has any. Even if |T|
    // doesn't, comparison between |T*| and |const volatile T*| is fine.
    return lhs.GetForComparison() ==
           static_cast<std::add_cv_t<T>*>(rhs.GetForComparison());
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs,
                                       const CheckedPtr<U, Impl>& rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs, U* rhs) {
    // Add |const volatile| when casting, in case |U| has any. Even if |T|
    // doesn't, comparison between |T*| and |const volatile T*| is fine.
    return lhs.GetForComparison() == static_cast<std::add_cv_t<T>*>(rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator==(U* lhs, const CheckedPtr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(U* lhs, const CheckedPtr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  // Needed for comparisons against nullptr. Without these, a slightly more
  // costly version would be called that extracts wrapped pointer, as opposed
  // to plain comparison against 0.
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs, std::nullptr_t) {
    return !lhs;
  }
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs, std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  friend ALWAYS_INLINE bool operator==(std::nullptr_t, const CheckedPtr& rhs) {
    return !rhs;
  }
  friend ALWAYS_INLINE bool operator!=(std::nullptr_t, const CheckedPtr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  friend ALWAYS_INLINE void swap(CheckedPtr& lhs, CheckedPtr& rhs) noexcept {
    Impl::IncrementSwapCountForTest();
    std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
  }

 private:
  // This getter is meant for situations where the pointer is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  ALWAYS_INLINE T* GetForDereference() const {
    return static_cast<T*>(Impl::SafelyUnwrapPtrForDereference(wrapped_ptr_));
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  ALWAYS_INLINE T* GetForExtraction() const {
    return static_cast<T*>(Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_));
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  ALWAYS_INLINE T* GetForComparison() const {
    return static_cast<T*>(Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_));
  }

  // Store the pointer as |uintptr_t|, because depending on implementation, its
  // unused bits may be re-purposed to store extra information.
  uintptr_t wrapped_ptr_;

  template <typename U, typename V>
  friend class CheckedPtr;
};

}  // namespace base

using base::CheckedPtr;

#endif  // BASE_MEMORY_CHECKED_PTR_H_
