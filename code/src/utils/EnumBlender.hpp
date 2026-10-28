/*
MIT License

Copyright (c) 2026 Vaclav Mach

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "dsp/math/qmath.hpp"
#include "utils/EnumTools.hpp"

namespace badger
{

/**
 * @class EnumBlender
 * @brief Crossfades enum-indexed values using a unipolar q15_t position.
 * @tparam EnumType Enum type containing COUNT value.
 */
template <typename EnumType>
class EnumBlender
{
public:
    /**
     * @brief Computes the two enum values and their blend ratio based on the given position.
     * @param position A q15_t value representing the blend position (0 to Q15_MAX).
     * @note The method calculates the two enum values (a_ and b_) and the blend ratio (ratio_ and ratio_inv_) based on the provided position.
     */
    constexpr void Compute(const q15_t position)
    {
        constexpr uint32_t kCount = static_cast<uint32_t>(std::to_underlying(EnumType::COUNT));
        static_assert(kCount >= 2, "EnumBlender requires enum COUNT >= 2");
        constexpr uint32_t kSegments = kCount - 1U;

        const q15_t clamped_pos = constrain(position, Q15_ZERO, Q15_MAX);
        const uint32_t scaled_pos = static_cast<uint32_t>(clamped_pos) * kSegments;

        uint32_t index_a = scaled_pos / static_cast<uint32_t>(Q15_MAX);
        if (index_a >= kCount)
        {
            index_a = kCount - 1U;
        }

        uint32_t index_b = index_a + 1U;
        if (index_b >= kCount)
        {
            index_b = kCount - 1U;
        }

        ratio_ = q15_div(static_cast<q15_t>(scaled_pos % static_cast<uint32_t>(Q15_MAX)), Q15_MAX);
        ratio_inv_ = q15_inv(ratio_);
        a_ = static_cast<EnumType>(index_a);
        b_ = static_cast<EnumType>(index_b);
    }

    template <size_t Size>
    /**
     * @brief Computes the two enum values and their blend ratio based on the given position.
     * @param position A q15_t value representing the blend position (0 to Q15_MAX).
     * @param items An array of enum values to blend between (doesn't have to be the full enum range).
     */
    constexpr void Compute(const q15_t position, const std::array<EnumType, Size> &items)
    {
        static_assert(Size >= 2, "EnumBlender requires at least two items");

        const q15_t clamped_pos = constrain(position, Q15_ZERO, Q15_MAX);
        const uint32_t scaled_pos = static_cast<uint32_t>(clamped_pos) * (static_cast<uint32_t>(Size) - 1U);

        uint32_t index_a = scaled_pos / static_cast<uint32_t>(Q15_MAX);
        if (index_a >= static_cast<uint32_t>(Size))
        {
            index_a = static_cast<uint32_t>(Size) - 1U;
        }

        uint32_t index_b = index_a + 1U;
        if (index_b >= static_cast<uint32_t>(Size))
        {
            index_b = static_cast<uint32_t>(Size) - 1U;
        }

        ratio_ = q15_div(static_cast<q15_t>(scaled_pos % static_cast<uint32_t>(Q15_MAX)), Q15_MAX);
        ratio_inv_ = q15_inv(ratio_);
        a_ = items[index_a];
        b_ = items[index_b];
    }

    template <typename T, typename BlendFn>
    /**
     * @brief Processes the previously computed enum values and their blend ratio using the provided blending function.
     * @param values An EnumArray containing the values to blend.
     * @param blend_fn A callable that takes two values and their blend ratios, returning the blended result.
     * @return The blended value of type T.
     */
    constexpr T Process(const EnumArray<EnumType, T> &values, BlendFn blend_fn) const
    {
        return blend_fn(values[a_], values[b_], ratio_, ratio_inv_);
    }

    /**
     * @brief Processes the previously computed enum values and their blend ratio for q15_t values.
     * @param values An EnumArray containing the q15_t values to blend.
     * @return The blended q15_t value.
     */
    constexpr q15_t Process(const EnumArray<EnumType, q15_t> &values) const
    {
        return q15_add(q15_mult(values[a_], ratio_inv_), q15_mult(values[b_], ratio_));
    }

    /**
     * @brief Processes the previously computed enum values and their blend ratio for q31_t values.
     * @param values An EnumArray containing the q31_t values to blend.
     * @return The blended q15_t value after converting from q31_t.
     */
    constexpr q15_t ProcessQ31(const EnumArray<EnumType, q31_t> &values) const
    {
        const q15_t a = q31_to_q15(values[a_]);
        const q15_t b = q31_to_q15(values[b_]);
        return q15_add(q15_mult(a, ratio_inv_), q15_mult(b, ratio_));
    }

private:
    EnumType a_ = static_cast<EnumType>(0);
    EnumType b_ = static_cast<EnumType>(0);
    q15_t ratio_ = 0;
    q15_t ratio_inv_ = Q15_MAX;
};

} // namespace badger