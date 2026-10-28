/*
MIT License

Copyright (c) 2024 Vaclav Mach (Bastl Instruments)
Copyright (c) 2024 Marek Mach (Bastl Instruments)

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

#include <cstdint>
#include "lookup_qmath_sine.hpp"
#include "math_utils.hpp"

namespace badger
{

/**
 * @file qmath.h
 * @ingroup dsp_math
 * @brief Fast fixed point math library.
 * @author Vaclav Mach (Bastl Instruments), Marek Mach (Bastl Instruments)
 * @date 2024-04-12
 *
 * This library provides fast fixed point arithmetic operations.
 * It includes functions and macros for addition, multiplication, and scaling of fixed point numbers.
 *
 */

/** @brief Fixed point 32-bit number (1-bit sign, 31-bit number) stored in 32 bits. */
using q31_t = int32_t;

static constexpr q31_t Q31_MAX = INT32_MAX;
static constexpr q31_t Q31_MIN = INT32_MIN;
static constexpr q31_t Q31_HALF = ((q31_t)0x3fffffffL);
static constexpr q31_t Q31_ZERO = ((q31_t)0u);

/**
 * @brief Constructs a Q1.31 fixed-point integer (`q31_t`) from a floating-point literal.
 *
 * @tparam T Must be `float` or `double`.
 * @param x A floating-point value in the range [-1.0, 1.0].
 * @return int32_t The fixed-point Q1.31 representation.
 *
 * @note
 * - Can only be used at compile time.
 * - The input must be a `float` or `double` literal (e.g., `q31(0.5f)` or `q31(0.5)`).
 * - `1.0` is mapped to INT32_MAX (2147483647).
 * - Values outside [-1.0, 1.0] will be constrained to this range.
 */
template <typename T>
consteval q31_t q31(const T x)
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                  "q31() requires float or double literal (e.g., q31(0.f) or q31(0.0))");
    float y = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
    if (y == 1.0f)
    {
        return std::numeric_limits<int32_t>::max();
    }
    return static_cast<q31_t>(y * 2147483648.0f);
}

/**
 * @brief Returns an inverted unipolar value in Q31 format.
 * @param x The input number, expected between 0 and 1 in Q31 format.
 * @return The inverted value (1-x), clamped to the 0 to 1 range.
 */
inline constexpr q31_t q31_inv(const q31_t x)
{
    if (x < 0)
    {
        return Q31_MAX;
    }
    else
    {
        return Q31_MAX - x;
    }
}

/**
 * @brief Saturates a fixed point number.
 */
inline constexpr q31_t q31_saturate(const int64_t x)
{
    return x > Q31_MAX ? Q31_MAX : ((x) < Q31_MIN ? Q31_MIN : (q31_t)x);
}

/** @brief Fixed point 16-bit number (1-bit sign, 15-bit number) stored in 16 bits to save RAM. */
using q15least_t = int16_t;

/** @brief Fixed point 16-bit number (1-bit sign, 15-bit number) stored in 32 bits to prevent padding overhead. */
using q15_t = int32_t;

/** @brief Fixed point 16-bit number in Q4.12 format (name kept for compatibility). */
using q4_12_t = int32_t;

static constexpr q15_t Q15_MAX = INT16_MAX;
static constexpr q15_t Q15_MIN = INT16_MIN;
static constexpr q15_t Q15_HALF = (q15_t)0x3fffL;
static constexpr q15_t Q15_ZERO = (q15_t)0u;

static constexpr q4_12_t Q4_12_MAX = INT16_MAX;
static constexpr q4_12_t Q4_12_MIN = INT16_MIN;
static constexpr q4_12_t Q4_12_ONE = (q4_12_t)0x1000;
static constexpr q4_12_t Q4_12_ZERO = (q4_12_t)0x0000;

/**
 * @brief Constructs a Q4.12 fixed-point integer (`q4_12_t`) from a floating-point literal.
 *
 * @tparam T Must be `float` or `double`.
 * @param x A floating-point value in the representable Q4.12 range.
 * @return q4_12_t The fixed-point Q4.12 representation.
 */
template <typename T>
consteval q4_12_t q4_12(const T x)
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                  "q4_12() requires float or double literal (e.g., q4_12(1.0f))");
    const float y = x > 7.999755859375f ? 7.999755859375f : (x < -8.0f ? -8.0f : x);
    return static_cast<q4_12_t>(y * 4096.0f);
}

/**
 * @brief Saturates a Q4.12 value represented in a wider integer.
 */
inline constexpr q4_12_t q4_12_saturate(const int32_t x)
{
    return x > Q4_12_MAX ? Q4_12_MAX : (x < Q4_12_MIN ? Q4_12_MIN : static_cast<q4_12_t>(x));
}

/**
 * @brief Adds two Q4.12 numbers with saturation.
 */
inline constexpr q4_12_t q4_12_add(const q4_12_t a, const q4_12_t b)
{
    const int32_t x = static_cast<int32_t>(a) + static_cast<int32_t>(b);
    return q4_12_saturate(x);
}

/**
 * @brief Subtracts two Q4.12 numbers with saturation.
 */
inline constexpr q4_12_t q4_12_sub(const q4_12_t a, const q4_12_t b)
{
    const int32_t x = static_cast<int32_t>(a) - static_cast<int32_t>(b);
    return q4_12_saturate(x);
}

/**
 * @brief Multiplies two Q4.12 numbers with saturation.
 */
inline constexpr q4_12_t q4_12_mult(const q4_12_t a, const q4_12_t b)
{
    int32_t x = static_cast<int32_t>(a) * static_cast<int32_t>(b);
    x = x >> 12;
    return q4_12_saturate(x);
}

/**
 * @brief Converts float to Q4.12.
 */
inline constexpr q4_12_t float_to_q4_12(const float a)
{
    const float value = constrain(a, -8.0f, 7.999755859375f);
    return static_cast<q4_12_t>(value * 4096.0f);
}

/**
 * @brief Converts Q4.12 to float.
 */
inline constexpr float q4_12_to_float(const q4_12_t a)
{
    return static_cast<float>(a) / 4096.0f;
}

/**
 * @brief Converts Q1.15 to Q4.12.
 */
inline constexpr q4_12_t q15_to_q4_12(const q15_t a)
{
    const int32_t clamped = constrain(a, Q15_MIN, Q15_MAX);
    int32_t numerator = clamped * static_cast<int32_t>(Q4_12_ONE);
    numerator += (numerator >= 0) ? (Q15_MAX / 2) : -(Q15_MAX / 2);
    return q4_12_saturate(numerator / Q15_MAX);
}

/**
 * @brief Converts Q4.12 to Q1.15 (saturated to [-1, 1]).
 */
inline constexpr q15_t q4_12_to_q15(const q4_12_t a)
{
    if (a >= Q4_12_ONE)
    {
        return Q15_MAX;
    }
    if (a <= -Q4_12_ONE)
    {
        return Q15_MIN;
    }

    int32_t x = static_cast<int32_t>(a) * Q15_MAX;
    x = x / static_cast<int32_t>(Q4_12_ONE);
    return x > Q15_MAX ? Q15_MAX : (x < Q15_MIN ? Q15_MIN : x);
}

/**
 * @brief Multiplies a Q1.15 signal by a Q4.12 gain and returns Q1.15.
 */
inline constexpr q15_t q15_mult_q4_12(const q15_t a, const q4_12_t b)
{
    int32_t x = static_cast<int32_t>(a) * static_cast<int32_t>(b);
    x = x >> 12;
    return x > Q15_MAX ? Q15_MAX : (x < Q15_MIN ? Q15_MIN : x);
}

/**
 * @brief Constructs a Q1.15 fixed-point integer (`q15_t`) from a floating-point literal.
 *
 * @tparam T Must be `float` or `double`.
 * @param x A floating-point value in the range [-1.0, 1.0].
 * @return int16_t The fixed-point Q1.15 representation.
 *
 * @note
 * - Can only be used at compile time.
 * - The input must be a `float` or `double` literal (e.g., `q15(0.25f)` or `q15(0.25)`).
 * - `1.0` is mapped to INT16_MAX (32767).
 * - Values outside [-1.0, 1.0] will be constrained to this range.
 */
template <typename T>
consteval q15_t q15(const T x)
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                  "q15() requires float or double literal (e.g., q15(0.f) or q15(0.0))");
    float y = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
    if (y == 1.0f)
    {
        return std::numeric_limits<int16_t>::max();
    }
    return static_cast<q15_t>(y * 32768.0f);
}

/**
 * @brief Creates a symmetric exponential Q15 map in range [-1, 1] at compile time.
 *
 * @tparam N Number of points in the map. Must be odd and >= 3.
 * @param curve_base Exponential base controlling curve steepness (minimum effective value is 2).
 *        Higher number -> higher curve (steeper response near the edges, flatter around zero).
 * @note Example for N=5 and curve_base=4:
 *       input  = {-1.0, -0.5, 0.0, 0.5, 1.0}
 *       output = {-1.0, -0.2, 0.0, 0.2, 1.0}
 */
template <size_t N>
consteval MapDef<q15_t, N> CreateQ15ExponentialMap(const uint32_t curve_base)
{
    static_assert(N >= 3 && (N % 2) == 1, "Map size must be odd and >= 3");

    const auto int_pow = [](const uint32_t base, const size_t exponent) consteval -> uint32_t
    {
        uint32_t result = 1;
        for (size_t i = 0; i < exponent; ++i)
        {
            result *= base;
        }
        return result;
    };

    const size_t mid_index = N / 2;
    const size_t half_steps = mid_index;
    const uint32_t base = (curve_base < 2) ? 2 : curve_base;
    const uint32_t max_raw = int_pow(base, half_steps) - 1;

    MapDef<q15_t, N> map{};

    for (size_t i = 0; i < N; ++i)
    {
        const float normalized_input = (static_cast<float>(i) / static_cast<float>(N - 1)) * 2.0f - 1.0f;
        map.input[i] = q15(normalized_input);
    }

    map.output[mid_index] = q15(0.0f);
    for (size_t step = 1; step <= half_steps; ++step)
    {
        const uint32_t raw = int_pow(base, step) - 1;
        const float normalized = static_cast<float>(raw) / static_cast<float>(max_raw);
        map.output[mid_index + step] = q15(normalized);
        map.output[mid_index - step] = q15(-normalized);
    }

    return map;
}

/**
 * @brief Saturates a fixed point number.
 */
inline constexpr q15_t q15_saturate(const q15_t x)
{
    return x > Q15_MAX ? Q15_MAX : (x < Q15_MIN ? Q15_MIN : x);
}

/**
 * @brief Returns an inverted unipolar value in Q15 format.
 * @param x The input number, expected between 0 and 1 in Q15 format.
 * @return The inverted value (1-x), clamped to the 0 to 1 range.
 */
inline constexpr q15_t q15_inv(const q15_t x)
{
    if (x < 0)
    {
        return Q15_MAX;
    }
    else if (x > Q15_MAX)
    {
        // This should never happen since we expect x to be valid Q15 number, but just in case...
        return 0;
    }
    else
    {
        return Q15_MAX - x;
    }
}

/**
 * @brief Converts a q15_t to a q31_t.
 */
inline constexpr q31_t q15_to_q31(const q15_t x)
{
    return ((q31_t)(x)) << 16;
}

/**
 * @brief Converts a q31_t to a q15_t.
 */
inline constexpr q15_t q31_to_q15(const q31_t x)
{
    return (x) >> 16;
}

/**
 * @brief Calculates the absolute value of a q15_t number. Handles problem with -32,768.
 * @note q15_t is stored as 32-bit integer so we don't need to cast it.
 *
 * @param x The input number.
 * @return The absolute value of the input number.
 */
inline constexpr q15_t q15_abs(const q15_t x)
{
    return q15_saturate(x > 0 ? x : -x);
}

/**
 * @brief Calculates the absolute value of a q31_t number. Handles problem with -2,147,483,648.
 * @param x The input number.
 * @return The absolute value of the input number.
 */
inline constexpr q31_t q31_abs(const q31_t x)
{
    if (x == Q31_MIN)
    {
        return Q31_MAX;
    }
    return x > 0 ? x : -x;
}

/**
 * @brief Adds two fixed point numbers saturating the result.
 *
 * This function adds two fixed point numbers and returns the result.
 * If the result exceeds the maximum or minimum value of the fixed point representation,
 * it is clamped to the maximum or minimum value respectively.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The sum of the two fixed point numbers.
 */
inline constexpr q31_t q31_add(const q31_t a, const q31_t b)
{
    int64_t x = (int64_t)a + (int64_t)b;
    return q31_saturate(x);
}

/**
 * @brief Adds multiple fixed point numbers saturating the result.
 *
 * This function adds multiple fixed point numbers by recursively calling the binary q31_add.
 * The result is saturated at each step to prevent overflow.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @param args Additional fixed point numbers to add.
 * @return The sum of all the fixed point numbers.
 */
template <typename... Args>
inline constexpr q31_t q31_add(const q31_t a, const q31_t b, const Args... args)
{
    static_assert(sizeof...(args) > 0, "q31_add variadic version requires at least 3 parameters");
    return q31_add(q31_add(a, b), args...);
}

/**
 * @brief Adds two fixed point numbers wrapping around on overflow instead of saturating.
 *
 * Meant for phase accumulators, which rely on the wrap-around to start the next cycle.
 * The addition is done in unsigned math, because signed overflow is undefined behavior in C++.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The wrapped sum of the two fixed point numbers.
 */
inline constexpr q31_t q31_add_wrap(const q31_t a, const q31_t b)
{
    return static_cast<q31_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/**
 * @brief Substract two fixed point numbers saturating the result.
 *
 * This function substract two fixed point numbers and returns the result.
 * If the result exceeds the maximum or minimum value of the fixed point representation,
 * it is clamped to the maximum or minimum value respectively.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The sum of the two fixed point numbers.
 */
inline constexpr q31_t q31_sub(const q31_t a, const q31_t b)
{
    int64_t x = (int64_t)a - (int64_t)b;
    return q31_saturate(x);
}

/**
 * @brief Multiplies two fixed point numbers saturating the result.
 *
 * This function multiplies two fixed point numbers and returns the result.
 * The result is right-shifted by 31 bits to maintain the fixed point representation.
 * If the result exceeds the maximum or minimum value of the fixed point representation,
 * it is clamped to the maximum or minimum value respectively.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The product of the two fixed point numbers.
 */
inline constexpr q31_t q31_mult(const q31_t a, const q31_t b)
{
    int64_t x = ((int64_t)a * (int64_t)b);
    x = x >> 31;
    return q31_saturate(x);
}

/**
 * @brief Multiplies multiple fixed point numbers saturating the result.
 *
 * This function multiplies multiple fixed point numbers by recursively calling the binary q31_mult.
 * The result is saturated at each step to prevent overflow.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @param args Additional fixed point numbers to multiply.
 * @return The product of all the fixed point numbers.
 */
template <typename... Args>
inline constexpr q31_t q31_mult(const q31_t a, const q31_t b, const Args... args)
{
    static_assert(sizeof...(args) > 0, "q31_mult variadic version requires at least 3 parameters");
    return q31_mult(q31_mult(a, b), args...);
}

/**
 * @brief Divides two fixed point numbers saturating the result.
 *
 * This function divides two fixed point numbers and returns the result.
 *
 * @param a The nominator.
 * @param b The denominator.
 * @return The result of the division.
 */
inline constexpr q31_t q31_div(const q31_t a, const q31_t b)
{
    if (b == 0)
    {
        return 0;
    }

    // Convert to int64_t to prevent overflow during the shift
    // Shift left to maintain precision after division
    int64_t a64 = static_cast<int64_t>(a) << 31;

    // Perform the division
    int64_t result = a64 / b;
    return q31_saturate(result);
}

/**
 * @brief Converts a float to a fixed point number.
 * @param a The float value to be converted.
 * @return The fixed point representation of the float value.
 */
inline constexpr q31_t float_to_q31(const float a)
{
    // Implement constrain logic inline to ensure it's always constexpr compatible
    float value = constrain(a, -1.0f, 1.0f);
    return static_cast<q31_t>(value * 2147483648.0f);
}

/**
 * @brief Converts a fixed point number to a float.
 * @param a The fixed point value to be converted.
 * @return The float representation of the fixed point value.
 */
inline constexpr float q31_to_float(const q31_t a)
{
    return static_cast<float>(a) / 2147483648.0f;
}

/**
 * @brief Converts a float to a fixed point number.
 * @param a The float value to be converted.
 * @return The fixed point representation of the float value.
 */
inline constexpr q15_t float_to_q15(const float a)
{
    float value = constrain(a, -1.0f, 1.0f);
    return static_cast<q15_t>(value * 32767.0f);
}

/**
 * @brief Converts a fixed point number to a float.
 * @param a The fixed point value to be converted.
 * @return The float representation of the fixed point value.
 */
inline constexpr float q15_to_float(const q15_t a)
{
    return static_cast<float>(a) / 32767.0f;
}

/**
 * @brief Scales an array of fixed point numbers.
 *
 * This function scales an array of fixed point numbers by a given scale factor and shift value.
 * The scale factor is multiplied with each element of the array, and the result is left-shifted
 * by the specified shift value. If the result exceeds the maximum or minimum value of the fixed
 * point representation, it is clamped to the maximum or minimum value respectively.
 *
 * @param buff The array of fixed point numbers to be scaled.
 * @param scale The scale factor.
 * @param shift The shift left value.
 * @param length The length of the array.
 */
inline constexpr void q31_scale_buffer(q31_t *buffer, const q31_t scale, const int8_t shift, const uint32_t length)
{
    int32_t shift_ = 31 - shift;
    for (uint32_t i = 0; i < length; i++)
    {
        int64_t x = ((int64_t)buffer[i] * (int64_t)scale);
        x = x >> shift_;
        buffer[i] = q31_saturate(x);
    }
}
/**
 * @brief Scales an array of fixed point numbers.
 *
 * This function scales an array of fixed point numbers by a given scale factor and shift value.
 * The scale factor is multiplied with each element of the array, and the result is left-shifted
 * by the specified shift value. If the result exceeds the maximum or minimum value of the fixed
 * point representation, it is clamped to the maximum or minimum value respectively.
 *
 * @param buff The array of fixed point numbers to be scaled.
 * @param scale The scale factor.
 * @param shift The shift left value.
 * @param length The length of the array.
 */
inline constexpr void q15_scale_buffer(q15_t *buffer, const q15_t scale, const int8_t shift, const uint32_t length)
{
    int32_t shift_ = 15 - shift;
    for (uint32_t i = 0; i < length; i++)
    {
        int32_t x = (buffer[i] * scale);
        x = x >> shift_;
        buffer[i] = q15_saturate(x);
    }
}

/**
 * @brief Adds two fixed point numbers saturating the result.
 *
 * This function adds two fixed point numbers and returns the result.
 * If the result exceeds the maximum or minimum value of the fixed point representation,
 * it is clamped to the maximum or minimum value respectively.
 *
 * @note q15_t is stored as 32-bit integer so we don't need to cast it.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The sum of the two fixed point numbers.
 */
inline constexpr q15_t q15_add(const q15_t a, const q15_t b)
{
    int32_t x = a + b;
    return q15_saturate(x);
}

/**
 * @brief Adds multiple fixed point numbers saturating the result.
 *
 * This function adds multiple fixed point numbers by recursively calling the binary q15_add.
 * The result is saturated at each step to prevent overflow.
 *
 * @note q15_t is stored as 32-bit integer so we don't need to cast it.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @param args Additional fixed point numbers to add.
 * @return The sum of all the fixed point numbers.
 */
template <typename... Args>
inline constexpr q15_t q15_add(const q15_t a, const q15_t b, const Args... args)
{
    static_assert(sizeof...(args) > 0, "q15_add variadic version requires at least 3 parameters");
    return q15_add(q15_add(a, b), args...);
}

/**
 * @brief Substract two fixed point numbers saturating the result.
 *
 * This function substracts two fixed point numbers and returns the result.
 * If the result exceeds the maximum or minimum value of the fixed point representation,
 * it is clamped to the maximum or minimum value respectively.
 *
 * @note q15_t is stored as 32-bit integer so we don't need to cast it

 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The sum of the two fixed point numbers.
 */
inline constexpr q15_t q15_sub(const q15_t a, const q15_t b)
{
    int32_t x = a - b;
    return q15_saturate(x);
}

/**
 * @brief Multiplies two fixed point numbers saturating the result.
 *
 * This function multiplies two fixed point numbers and returns the result.
 * The result is right-shifted by 15 bits to maintain the fixed point representation.
 * If the result exceeds the maximum or minimum value of the fixed point representation,
 * it is clamped to the maximum or minimum value respectively.
 *
 * @note q15_t is stored as 32-bit integer so we don't need to cast it

 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The product of the two fixed point numbers.
 */
inline constexpr q15_t q15_mult(const q15_t a, const q15_t b)
{
    int32_t x = (a * b);
    x = x >> 15;
    return q15_saturate(x);
}

/**
 * @brief Multiplies multiple fixed point numbers saturating the result.
 *
 * This function multiplies multiple fixed point numbers by recursively calling the binary q15_mult.
 * The result is saturated at each step to prevent overflow.
 *
 * @note q15_t is stored as 32-bit integer so we don't need to cast it.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @param args Additional fixed point numbers to multiply.
 * @return The product of all the fixed point numbers.
 */
template <typename... Args>
inline constexpr q15_t q15_mult(const q15_t a, const q15_t b, const Args... args)
{
    static_assert(sizeof...(args) > 0, "q15_mult variadic version requires at least 3 parameters");
    return q15_mult(q15_mult(a, b), args...);
}

/**
 * @brief Multiplies two fixed point numbers WITHOUT SATURATION CHECK!
 *
 * This function multiplies two fixed point numbers and returns the result.
 * The result is right-shifted by 15 bits to maintain the fixed point representation.
 *
 * @note q15_t is stored as 32-bit integer so we don't need to cast it.
 *
 * @param a The first fixed point number.
 * @param b The second fixed point number.
 * @return The product of the two fixed point numbers.
 */
inline constexpr q15_t q15_mult_fast(const q15_t a, const q15_t b)
{
    return (a * b) >> 15;
}

/**
 * @brief Modulates a native q15 oscillator frequency by a bipolar modulation source.
 *
 * Uses fixed-point scale math:
 * frequency_out = base_frequency * (1 + modulation * (add + multiply * depth))
 *
 * @param base_frequency Base native frequency in q15_t format.
 * @param modulation Bipolar modulation in q15_t format (-1..1).
 * @param depth Unipolar modulation depth in q15_t format (0..1), e.g. from pot_to_q15().
 * @param add Base amount added to modulation depth (integer scalar).
 * @param multiply Multiplier for depth amount (integer scalar).
 * @return Modulated native frequency in q15_t format.
 */
inline constexpr q15_t q15_freq_mod(const q15_t base_frequency,
                                    const q15_t modulation,
                                    const q15_t depth,
                                    const int32_t add,
                                    const int32_t multiply)
{
    static constexpr int32_t kQ12One = 1 << 12;
    const int32_t safe_depth = constrain(depth, 0, Q15_MAX);
    const int32_t modulation_depth_q12 = (add * kQ12One) + ((multiply * safe_depth) >> 3);
    const int32_t modulation_q12 = (static_cast<int32_t>(modulation) * modulation_depth_q12) >> 15;
    const int32_t scale_q12 = kQ12One + modulation_q12;
    const int32_t out = (static_cast<int32_t>(base_frequency) * scale_q12) >> 12;
    return q15_saturate(out);
}

/**
 * @brief Modulates a native q31 oscillator frequency by a bipolar modulation source.
 * @details Same as q15_freq_mod(), for oscillators that take a q31_t native frequency.
 *
 * @param base_frequency Base native frequency in q31_t format.
 * @param modulation Bipolar modulation in q15_t format (-1..1).
 * @param depth Unipolar modulation depth in q15_t format (0..1), e.g. from pot_to_q15().
 * @param add Base amount added to modulation depth (integer scalar).
 * @param multiply Multiplier for depth amount (integer scalar).
 * @return Modulated native frequency in q31_t format, never negative.
 */
inline constexpr q31_t q31_freq_mod(const q31_t base_frequency,
                                    const q15_t modulation,
                                    const q15_t depth,
                                    const int32_t add,
                                    const int32_t multiply)
{
    static constexpr int32_t kQ12One = 1 << 12;
    const int32_t safe_depth = constrain(depth, 0, Q15_MAX);
    const int32_t modulation_depth_q12 = (add * kQ12One) + ((multiply * safe_depth) >> 3);
    const int32_t modulation_q12 = (static_cast<int32_t>(modulation) * modulation_depth_q12) >> 15;
    const int32_t scale_q12 = kQ12One + modulation_q12;
    const int64_t out = (static_cast<int64_t>(base_frequency) * scale_q12) >> 12;
    return out < 0 ? Q31_ZERO : q31_saturate(out);
}

/**
 * @brief Divides two fixed point numbers saturating the result.
 *
 * This function divides two fixed point numbers and returns the result.

 * @note q15_t is stored as 32-bit integer so we don't need to cast it.

 *
 * @param a The nominator.
 * @param b The denominator.
 * @return The result of the division.
 */
inline constexpr q15_t q15_div(const q15_t a, const q15_t b)
{
    // Handle division by zero
    if (b == 0)
    {
        return 0;
    }
    // Convert to int32_t to prevent overflow during the shift
    // Shift left to maintain precision after division
    int32_t a32 = a << 15;
    int32_t result = a32 / b;
    return q15_saturate(result);
}

/**
 * @brief Converts a fraction of two numbers into Q15 fixed point format.
 *
 * @param a The nominator, must be smaller than The denominator.
 * @param b The denominator.
 * @return The number between 0-1 in Q15 format.
 */
inline constexpr q15_t fraction_to_q15(const int32_t numerator, const int32_t denominator)
{
    int32_t result = (numerator * 32767 + (denominator / 2)) / denominator;
    return q15_saturate(result);
}

/**
 * @brief Fast sine implementation using an interpolated lookup table.
 * @details The table index comes from the top bits of the phase and the remaining bits linearly
 *          interpolate towards the next entry, so the output resolution is set by the q15 table
 *          values rather than by the table length. Without that interpolation the wave comes out
 *          as audible steps, which the DAC then faithfully reproduces.
 * @param x The angle in q31_t format, where the full q31 range is one period.
 * @return The sine of the angle in q31_t format.
 */
inline constexpr q31_t q31_sine(const q31_t x)
{
    const uint32_t phase = static_cast<uint32_t>(q31_abs(x));
    const size_t index = phase >> QMATH_SINE_TABLE_SHIFT_Q31;
    const int32_t fraction = static_cast<int32_t>((phase >> (QMATH_SINE_TABLE_SHIFT_Q31 - 15)) & 0x7FFF);

    const int32_t current = qmath_sine_table[index];
    const int32_t next = qmath_sine_table[(index + 1u) & QMATH_SINE_TABLE_MASK];

    return (current + (((next - current) * fraction) >> 15)) << 16;
}

/**
 * @brief Fast sine implementation using an interpolated lookup table.
 * @details See q31_sine(); this variant only has the q15 phase bits to interpolate with.
 * @param x The angle in q15_t format, where the full q15 range is one period.
 * @return The sine of the angle in q15_t format.
 */
inline constexpr q15_t q15_sine(const q15_t x)
{
    const uint32_t phase = static_cast<uint32_t>(q15_abs(x));
    const size_t index = phase >> QMATH_SINE_TABLE_SHIFT_Q15;
    const int32_t fraction = static_cast<int32_t>(phase & ((1u << QMATH_SINE_TABLE_SHIFT_Q15) - 1u));

    const int32_t current = qmath_sine_table[index];
    const int32_t next = qmath_sine_table[(index + 1u) & QMATH_SINE_TABLE_MASK];

    return current + (((next - current) * fraction) >> QMATH_SINE_TABLE_SHIFT_Q15);
}

/**
 * @brief Converts a frequency in Hz to a relative frequency to sample_rate.
 * @param frequency The frequency in Hz.
 * @param sample_rate The sample rate in Hz.
 * @return The relative frequency in q31_t format.
 */
inline constexpr q31_t freq_to_q31(const float frequency, const float sample_rate)
{
    return static_cast<q31_t>(frequency * (static_cast<float>(Q31_MAX) / sample_rate));
}

/**
 * @brief Converts a frequency in Hz to a relative frequency to sample_rate.
 * @param frequency The frequency in Hz.
 * @param sample_rate The sample rate in Hz.
 * @return The relative frequency in q15_t format.
 */
inline constexpr q15_t freq_to_q15(const float frequency, const float sample_rate)
{
    return static_cast<q15_t>(frequency * (static_cast<float>(Q15_MAX) / sample_rate));
}

/**
 * @brief Converts number from potentiometer range (0-4095) to q15_t (0-32768).
 * @param input The potentiometer value (0-4095)
 * @return The q15_t representation of the potentiometer value (0-1)
 */
inline constexpr q15_t pot_to_q15(const uint16_t input)
{
    int32_t result = constrain(input, 0, 4095);
    return result << 3;
}

/**
 * @brief Converts number from potentiometer range (0-4095) to float (0.0f-1.0f).
 * @param input The potentiometer value (0-4095)
 * @return The float representation of the potentiometer value (0-1)
 */
inline constexpr float pot_to_float(const uint16_t input)
{
    int32_t result = constrain(input, 0, 4095);
    return static_cast<float>(result) / 4095.0f;
}

/**
 * @brief Converts a bipolar q15_t value (-1 to 1) to a unipolar q15_t value (0 to 1).
 * @param x The input bipolar q15_t value.
 * @return The unipolar q15_t value.
 */
inline constexpr q15_t q15_unipolar(const q15_t x)
{
    const q15_t clamped = constrain(x, Q15_MIN, Q15_MAX);
    return q15_saturate((clamped - Q15_MIN) >> 1);
}

/**
 * @brief Returns the minimum of two q15_t values.
 * @param x The first q15_t value.
 * @param y The second q15_t value.
 * @return The minimum of the two q15_t values.
 */
inline constexpr q15_t q15_min(const q15_t x, const q15_t y)
{
    return std::min(x, y);
}

template <typename... Args>
inline constexpr q15_t q15_min(const q15_t x, const q15_t y, const Args... args)
{
    static_assert(sizeof...(args) > 0, "q15_min variadic version requires at least 3 parameters");
    return q15_min(q15_min(x, y), args...);
}

/**
 * @brief Returns the maximum of two q15_t values.
 * @param x The first q15_t value.
 * @param y The second q15_t value.
 * @return The maximum of the two q15_t values.
 */
inline constexpr q15_t q15_max(const q15_t x, const q15_t y)
{
    return std::max(x, y);
}

template <typename... Args>
inline constexpr q15_t q15_max(const q15_t x, const q15_t y, const Args... args)
{
    static_assert(sizeof...(args) > 0, "q15_max variadic version requires at least 3 parameters");
    return q15_max(q15_max(x, y), args...);
}

} // namespace badger
