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
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace badger
{

/**
 * @file lookup_qmath_sine.hpp
 * @ingroup dsp_math
 * @brief Sine table used by the fast sine lookups in qmath.hpp.
 * @details
 * The table stores one full period as signed 16-bit samples and is meant to be read with
 * linear interpolation (see q31_sine()/q15_sine()), which is what keeps the reconstructed
 * wave smooth: the table resolution sets the interpolation error, not the output resolution.
 *
 * Sizing rationale: at 1024 entries the interpolated harmonic distortion sits far below the
 * 12-bit DAC's own noise floor, so the oscillators are no longer the limiting factor in the
 * audio path. Change QMATH_SINE_TABLE_SIZE_EXP to trade flash for accuracy.
 *
 * @note The table is constexpr, so it lives in flash and costs no RAM.
 * @author Vaclav Mach
 * @date 2026-09-02
 */

static constexpr size_t QMATH_SINE_TABLE_SIZE_EXP = 10u;
static constexpr size_t QMATH_SINE_TABLE_SIZE = 1u << QMATH_SINE_TABLE_SIZE_EXP;
static constexpr size_t QMATH_SINE_TABLE_MASK = QMATH_SINE_TABLE_SIZE - 1u;

/** @brief Phase bits below the table index, available as the interpolation fraction. */
static constexpr int32_t QMATH_SINE_TABLE_SHIFT_Q31 = 31 - static_cast<int32_t>(QMATH_SINE_TABLE_SIZE_EXP);
static constexpr int32_t QMATH_SINE_TABLE_SHIFT_Q15 = 15 - static_cast<int32_t>(QMATH_SINE_TABLE_SIZE_EXP);

static_assert(QMATH_SINE_TABLE_SHIFT_Q15 > 0, "Sine table is too large for the q15 phase resolution");
static_assert(QMATH_SINE_TABLE_SHIFT_Q31 > 15, "Sine table is too large for the q31 interpolation fraction");

namespace detail
{
/**
 * @brief Compile-time sine of a phase expressed in turns (1.0 is a full period).
 * @details std::sin() is not usable in a constant expression, so the table is generated with a
 *          Taylor series. The phase is folded into a quarter period first, which keeps the
 *          series argument below pi/2 where nine terms already reach double precision.
 * @param turns Phase in the range 0.0 to 1.0.
 * @return Sine of the given phase.
 */
inline constexpr double ConstexprSineTurns(double turns)
{
    bool negate = false;

    if (turns > 0.5)
    {
        turns -= 0.5;
        negate = true;
    }
    if (turns > 0.25)
    {
        turns = 0.5 - turns;
    }

    const double x = turns * 2.0 * std::numbers::pi_v<double>;
    const double x_squared = x * x;

    double term = x;
    double sum = x;
    for (int32_t n = 1; n < 9; ++n)
    {
        const double k = static_cast<double>(2 * n) * static_cast<double>(2 * n + 1);
        term = -term * x_squared / k;
        sum += term;
    }

    return negate ? -sum : sum;
}
} // namespace detail

/**
 * @brief One period of a sine wave in q15 scale, indexed by the top bits of the phase.
 */
inline constexpr std::array<int16_t, QMATH_SINE_TABLE_SIZE> qmath_sine_table = []
{
    std::array<int16_t, QMATH_SINE_TABLE_SIZE> table{};

    for (size_t i = 0u; i < QMATH_SINE_TABLE_SIZE; ++i)
    {
        const double turns = static_cast<double>(i) / static_cast<double>(QMATH_SINE_TABLE_SIZE);
        const double scaled = detail::ConstexprSineTurns(turns) * 32767.0;
        table[i] = static_cast<int16_t>(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
    }

    return table;
}();

} // namespace badger
