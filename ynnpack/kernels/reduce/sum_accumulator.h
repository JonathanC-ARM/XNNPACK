// Copyright 2025 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#ifndef XNNPACK_YNNPACK_KERNELS_REDUCE_SUM_ACCUMULATOR_H_
#define XNNPACK_YNNPACK_KERNELS_REDUCE_SUM_ACCUMULATOR_H_

#include <cassert>
#include <cstddef>
#include <type_traits>

#include "ynnpack/base/arithmetic.h"
#include "ynnpack/base/base.h"
#include "ynnpack/base/simd/vec.h"

namespace ynn {

using simd::transpose;
using simd::extract;

template <typename AccT, typename AT>
AccT reduce_add(AccT acc, AT a,
                std::integral_constant<size_t, 1>/*horizontal_factor*/) {
  return acc += a;
}

template <typename AccT>
auto sum_rows(AccT acc[4], std::integral_constant<size_t, 16> /*K*/) {
  using OutAccT = simd::vec<typename AccT::value_type, 4>;
  auto v_0 = (extract<0>(acc[0], OutAccT{}) + extract<1>(acc[0], OutAccT{})) +
             (extract<2>(acc[0], OutAccT{}) + extract<3>(acc[0], OutAccT{}));
  auto v_1 = (extract<0>(acc[1], OutAccT{}) + extract<1>(acc[1], OutAccT{})) +
             (extract<2>(acc[1], OutAccT{}) + extract<3>(acc[1], OutAccT{}));
  auto v_2 = (extract<0>(acc[2], OutAccT{}) + extract<1>(acc[2], OutAccT{})) +
             (extract<2>(acc[2], OutAccT{}) + extract<3>(acc[2], OutAccT{}));
  auto v_3 = (extract<0>(acc[3], OutAccT{}) + extract<1>(acc[3], OutAccT{})) +
             (extract<2>(acc[3], OutAccT{}) + extract<3>(acc[3], OutAccT{}));

  auto t = transpose<typename AccT::value_type>(
      {{v_0, v_1, v_2, v_3}});
  return (t[0] + t[1]) + (t[2] + t[3]);
}

template <typename AccT>
auto sum_rows(AccT acc[4], std::integral_constant<size_t, 8> /*K*/) {
  using OutAccT = simd::vec<typename AccT::value_type, 4>;
  auto low = transpose<typename AccT::value_type>({{
      extract<0>(acc[0], OutAccT{}), extract<0>(acc[1], OutAccT{}),
      extract<0>(acc[2], OutAccT{}), extract<0>(acc[3], OutAccT{}),
  }});
  auto high = transpose<typename OutAccT::value_type>({{
      extract<1>(acc[0], OutAccT{}), extract<1>(acc[1], OutAccT{}),
      extract<1>(acc[2], OutAccT{}), extract<1>(acc[3], OutAccT{}),
  }});
  return ((low[0] + high[0]) + (low[1] + high[1])) +
      ((low[2] + high[2]) + (low[3] + high[3]));
}

template <typename AccT>
auto sum_rows(AccT acc[4], std::integral_constant<size_t, 4>/*K*/) {
  auto t = transpose<typename AccT::value_type>(
      {acc[0], acc[1], acc[2], acc[3]});

  return (t[0] + t[1]) + (t[2] + t[3]);
}

template <typename AccT, size_t K_>
struct sum_accumulator_x32 {
  static constexpr std::integral_constant<size_t, 4> N = {};
  static constexpr std::integral_constant<size_t, K_> K = {};
  static constexpr std::integral_constant<size_t, K / AccT::N>
      horizontal_factor = {};

  AccT acc[N];

  sum_accumulator_x32() = default;

  YNN_ALWAYS_INLINE explicit sum_accumulator_x32(size_t) {
    AccT zero(0);

    for (size_t i = 0; i < N; ++i) {
      acc[i] = zero;
    }
  }

  template <typename AT, typename NT, typename KT>
  YNN_ALWAYS_INLINE void reduce(const AT* A, size_t A_stride_n,
                                NT n, KT k) {
    const simd::vec<AT, K> zero(0);
    auto a_0 = load(offset_bytes(A, 0 * A_stride_n), zero, k);
    auto a_1 = 1 < n ? load(offset_bytes(A, 1 * A_stride_n), zero, k) : zero;
    auto a_2 = 2 < n ? load(offset_bytes(A, 2 * A_stride_n), zero, k) : zero;
    auto a_3 = 3 < n ? load(offset_bytes(A, 3 * A_stride_n), zero, k) : zero;

    acc[0] = reduce_add(acc[0], a_0, horizontal_factor);
    acc[1] = reduce_add(acc[1], a_1, horizontal_factor);
    acc[2] = reduce_add(acc[2], a_2, horizontal_factor);
    acc[3] = reduce_add(acc[3], a_3, horizontal_factor);
  }

  template <typename T, typename NT>
  YNN_ALWAYS_INLINE void accumulate(size_t /*C_stride_m*/, T* __restrict C,
                                    NT n) {
    static_assert(N == 4);
    using OutAccT = simd::vec<T, N>;

    store(C, load(C, OutAccT{}, n) + sum_rows<AccT>(acc, AccT::N), n);
  }
};

template <typename InVT, typename AccT>
struct sum_accumulator_k1_1 {
  static constexpr std::integral_constant<size_t, 1> K2 = {};
  static constexpr std::integral_constant<size_t, AccT::N> N = {};

  AccT acc;

  sum_accumulator_k1_1() = default;

  YNN_ALWAYS_INLINE explicit sum_accumulator_k1_1(size_t) {
    acc = AccT(0);
  }

  template <typename NT, typename K2T>
  YNN_ALWAYS_INLINE void reduce(const typename InVT::value_type* A, NT n,
                                size_t A_stride_k2, K2T k2) {
    assert(k2 <= K2);
    assert(n <= N);
    assert(n > 0);

    acc += load(offset_bytes(A, 0 * A_stride_k2), InVT(0), n);
  }

  template <typename T, typename NT>
  YNN_ALWAYS_INLINE void accumulate(size_t /*C_stride_m*/, T* __restrict C,
                                    NT n) {
    assert(n <= N);
    assert(n > 0);

    store(C, load(C, AccT{}, n) + acc, n);
  }
};

}  // namespace ynn

#endif  // XNNPACK_YNNPACK_KERNELS_REDUCE_SUM_ACCUMULATOR_H_
