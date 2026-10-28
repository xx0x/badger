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

#include <cstddef>
#include <cstdint>
#include "dsp/math/qmath.hpp"

namespace badger
{

/**
 * @class BlockRamp
 * @ingroup dsp_control
 * @brief Linear ramp across one audio block, between two values that only change once per block.
 * @details Control signals like envelopes run at the block rate. Applying each new value to a whole
 *          block at once turns their curves into a staircase that buzzes at the block rate, ramping
 *          from the previous value to the new one across the block smooths that out.
 * @author Vaclav Mach
 * @date 2026-10-17
 */
class BlockRamp
{
public:
    /**
     * @brief Sets up a ramp that reaches `to` on the last of `samples` Next() calls.
     * @param from Value at the end of the previous block.
     * @param to Value at the end of this block.
     * @param samples Number of samples in the block.
     */
    constexpr BlockRamp(q15_t from, q15_t to, size_t samples)
        : accumulator_(from * kOne),
          step_(((to - from) * kOne) / static_cast<int32_t>(samples > 0 ? samples : 1))
    {
    }

    /**
     * @brief Returns the ramp value for the next sample.
     */
    constexpr q15_t Next()
    {
        accumulator_ += step_;
        return accumulator_ / kOne;
    }

private:
    // Fractional bits of the accumulator, so slow ramps don't round down to no movement at all
    static constexpr int32_t kOne = 1 << 12;

    int32_t accumulator_;
    int32_t step_;
};

} // namespace badger
