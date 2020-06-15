// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Block transpose for DCT/IDCT

#if defined(JXL_TRANSPOSE_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef JXL_TRANSPOSE_INL_H_
#undef JXL_TRANSPOSE_INL_H_
#else
#define JXL_TRANSPOSE_INL_H_
#endif

#include <stddef.h>

#include "jxl/base/status.h"
#include "jxl/dct_block-inl.h"

// SIMD code
#include <hwy/before_namespace-inl.h>
namespace jxl {
#include <hwy/begin_target-inl.h>

#ifndef JXL_INLINE_TRANSPOSE
// Workaround for issue #42 - (excessive?) inlining causes invalid codegen.
#if defined(__arm__)
#define JXL_INLINE_TRANSPOSE HWY_NOINLINE
#else
#define JXL_INLINE_TRANSPOSE HWY_INLINE
#endif
#endif  // JXL_INLINE_TRANSPOSE

template <bool enabled>
struct TransposeSimdTag {};

template <size_t ROWS, size_t COLS, class From, class To>
JXL_INLINE_TRANSPOSE void GenericTransposeBlock(TransposeSimdTag<false>,
                                                const From& from,
                                                const To& to) {
  for (size_t n = 0; n < ROWS; ++n) {
    for (size_t m = 0; m < COLS; ++m) {
      to.Write(from.Read(n, m), m, n);
    }
  }
}

#if HWY_CAP_GE256
constexpr bool TransposeUseSimd(size_t ROWS, size_t COLS) {
  return ROWS % 8 == 0 && COLS % 8 == 0;
}

template <size_t ROWS, size_t COLS, class From, class To>
JXL_INLINE_TRANSPOSE void GenericTransposeBlock(TransposeSimdTag<true>,
                                                const From& from,
                                                const To& to) {
  static_assert(MaxLanes(BlockDesc<8>()) == 8, "Invalid descriptor size");
  static_assert(ROWS % 8 == 0, "Invalid number of rows");
  static_assert(COLS % 8 == 0, "Invalid number of columns");
  for (size_t n = 0; n < ROWS; n += 8) {
    for (size_t m = 0; m < COLS; m += 8) {
      auto i0 = from.LoadPart(BlockDesc<8>(), n + 0, m + 0);
      auto i1 = from.LoadPart(BlockDesc<8>(), n + 1, m + 0);
      auto i2 = from.LoadPart(BlockDesc<8>(), n + 2, m + 0);
      auto i3 = from.LoadPart(BlockDesc<8>(), n + 3, m + 0);
      auto i4 = from.LoadPart(BlockDesc<8>(), n + 4, m + 0);
      auto i5 = from.LoadPart(BlockDesc<8>(), n + 5, m + 0);
      auto i6 = from.LoadPart(BlockDesc<8>(), n + 6, m + 0);
      auto i7 = from.LoadPart(BlockDesc<8>(), n + 7, m + 0);
      // Surprisingly, this straightforward implementation (24 cycles on port5)
      // is faster than load128+insert and LoadDup128+ConcatUpperLower+blend.
      const auto q0 = InterleaveLower(i0, i2);
      const auto q1 = InterleaveLower(i1, i3);
      const auto q2 = InterleaveUpper(i0, i2);
      const auto q3 = InterleaveUpper(i1, i3);
      const auto q4 = InterleaveLower(i4, i6);
      const auto q5 = InterleaveLower(i5, i7);
      const auto q6 = InterleaveUpper(i4, i6);
      const auto q7 = InterleaveUpper(i5, i7);

      const auto r0 = InterleaveLower(q0, q1);
      const auto r1 = InterleaveUpper(q0, q1);
      const auto r2 = InterleaveLower(q2, q3);
      const auto r3 = InterleaveUpper(q2, q3);
      const auto r4 = InterleaveLower(q4, q5);
      const auto r5 = InterleaveUpper(q4, q5);
      const auto r6 = InterleaveLower(q6, q7);
      const auto r7 = InterleaveUpper(q6, q7);

      i0 = ConcatLowerLower(r4, r0);
      i1 = ConcatLowerLower(r5, r1);
      i2 = ConcatLowerLower(r6, r2);
      i3 = ConcatLowerLower(r7, r3);
      i4 = ConcatUpperUpper(r4, r0);
      i5 = ConcatUpperUpper(r5, r1);
      i6 = ConcatUpperUpper(r6, r2);
      i7 = ConcatUpperUpper(r7, r3);
      to.StorePart(BlockDesc<8>(), i0, m + 0, n + 0);
      to.StorePart(BlockDesc<8>(), i1, m + 1, n + 0);
      to.StorePart(BlockDesc<8>(), i2, m + 2, n + 0);
      to.StorePart(BlockDesc<8>(), i3, m + 3, n + 0);
      to.StorePart(BlockDesc<8>(), i4, m + 4, n + 0);
      to.StorePart(BlockDesc<8>(), i5, m + 5, n + 0);
      to.StorePart(BlockDesc<8>(), i6, m + 6, n + 0);
      to.StorePart(BlockDesc<8>(), i7, m + 7, n + 0);
    }
  }
}
#elif HWY_TARGET != HWY_SCALAR
constexpr bool TransposeUseSimd(size_t ROWS, size_t COLS) {
  return ROWS % 4 == 0 && COLS % 4 == 0;
}
template <size_t ROWS, size_t COLS, class From, class To>
JXL_INLINE_TRANSPOSE void GenericTransposeBlock(TransposeSimdTag<true>,
                                                const From& from,
                                                const To& to) {
  static_assert(MaxLanes(BlockDesc<4>()) == 4, "Invalid descriptor size");
  static_assert(ROWS % 4 == 0, "Invalid number of rows");
  static_assert(COLS % 4 == 0, "Invalid number of columns");
  for (size_t n = 0; n < ROWS; n += 4) {
    for (size_t m = 0; m < COLS; m += 4) {
      const auto p0 = from.LoadPart(BlockDesc<4>(), n + 0, m + 0);
      const auto p1 = from.LoadPart(BlockDesc<4>(), n + 1, m + 0);
      const auto p2 = from.LoadPart(BlockDesc<4>(), n + 2, m + 0);
      const auto p3 = from.LoadPart(BlockDesc<4>(), n + 3, m + 0);

      const auto q0 = InterleaveLower(p0, p2);
      const auto q1 = InterleaveLower(p1, p3);
      const auto q2 = InterleaveUpper(p0, p2);
      const auto q3 = InterleaveUpper(p1, p3);

      const auto r0 = InterleaveLower(q0, q1);
      const auto r1 = InterleaveUpper(q0, q1);
      const auto r2 = InterleaveLower(q2, q3);
      const auto r3 = InterleaveUpper(q2, q3);

      to.StorePart(BlockDesc<4>(), r0, m + 0, n + 0);
      to.StorePart(BlockDesc<4>(), r1, m + 1, n + 0);
      to.StorePart(BlockDesc<4>(), r2, m + 2, n + 0);
      to.StorePart(BlockDesc<4>(), r3, m + 3, n + 0);
    }
  }
}
#else
constexpr bool TransposeUseSimd(size_t ROWS, size_t COLS) { return false; }
#endif

template <size_t N, size_t M>
struct Transpose {
  template <typename From, typename To>
  static void Run(const From& from, const To& to) {
    // This does not guarantee anything, just saves from the most stupid
    // mistakes.
    JXL_DASSERT(from.Address(0, 0) != to.Address(0, 0));
    TransposeSimdTag<TransposeUseSimd(N, M)> tag;
    GenericTransposeBlock<N, M>(tag, from, to);
  }
};
#include <hwy/end_target-inl.h>
}  // namespace jxl
#include <hwy/after_namespace-inl.h>

#endif  // include guard