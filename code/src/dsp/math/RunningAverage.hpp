/*
MIT License

Copyright (c) 2024 Vaclav Mach (Bastl Instruments)
Copyright (c) 2025 Marek Mach (Bastl Instruments)

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

#include <algorithm>
#include <array>
#include <cstddef>

namespace badger
{

/**
 * @class RunningAverage
 * @ingroup dsp_math
 * @brief Calculating the running average and median of a set of values in a circular buffer.
 * @details Buffer capacity is fixed at compile time (template parameter N) so the storage is
 *          inline and no heap allocation is used - important on targets with a tiny/no heap.
 *          Add values with Add() method. Optionally set the resetting threshold with
 *          SetResetThreshold() method for instantaneous response to larger changes.
 * @tparam T Usually float or int32_t etc.
 * @tparam N Capacity of the circular buffer.
 * @author Vaclav Mach (Bastl Instruments), Marek Mach (Bastl Instruments)
 * @date 2024-07-31
 */
template <typename T, std::size_t N>
class RunningAverage
{
public:
    static_assert(N > 0, "RunningAverage capacity must be greater than 0");

    /**
     * @brief Construct a new Running Average object
     * @param reset_threshold The threshold for resetting the buffer. If the difference between the current average and the new value exceeds this threshold, the buffer is reset.
     * @note If reset_threshold is set to 0, the resetting is disabled.
     */
    explicit RunningAverage(T reset_threshold = T{})
    {
        SetResetThreshold(reset_threshold);
    }

    /**
     * @brief Sets the resetting threshold.
     * @param threshold The resetting threshold value, setting it to 0 disables resetting.
     */
    void SetResetThreshold(T threshold)
    {
        reset_threshold_ = threshold;
        reset_threshold_set_ = true;
        if constexpr (std::is_floating_point_v<T>)
        {
            if (threshold <= 0.0f)
            {
                reset_threshold_set_ = false; // disable resetting if threshold is zero or negative
            }
        }
        else if constexpr (std::is_signed_v<T>)
        {
            if (threshold <= 0)
            {
                reset_threshold_set_ = false; // disable resetting if threshold is zero or negative
            }
        }
        else
        {
            if (threshold <= 0)
            {
                reset_threshold_set_ = false; // disable resetting if threshold is zero
            }
        }
    }

    /**
     * @brief Resets the buffer.
     */
    void Reset()
    {
        count_ = 0;
        index_ = 0;
    }

    /**
     * @brief Checks if the buffer is empty.
     * @return True if the buffer is empty, false otherwise.
     */
    bool IsEmpty() const
    {
        return count_ == 0;
    }

    /**
     * @brief Gets the count of the items (maxes out at size).
     * @return The count of the items.
     */
    size_t GetCount() const
    {
        return count_;
    }

    /**
     * @brief Adds a value to the circular buffer.
     * @param value Usually float or int32_t etc.
     */
    void Add(T value)
    {
        if (reset_threshold_set_)
        {
            T avg = GetAverage();
            T diff = (value > avg) ? (value - avg) : (avg - value);
            if (diff > reset_threshold_)
            {
                Reset();
            }
        }
        values_[index_] = value;
        index_ = (index_ + 1) % N;
        if (count_ < N)
        {
            count_++;
        }
    }

    /**
     * @brief Gets the average of the values in the circular buffer.
     * @return The average of the values.
     */
    T GetAverage() const
    {
        T sum = 0;
        for (size_t i = 0; i < count_; i++)
        {
            sum += values_[i];
        }
        return count_ > 0 ? sum / count_ : 0;
    }

    /**
     * @brief Gets the median of the values in the circular buffer.
     * @return The median of the values.
     */
    T GetMedian() const
    {
        if (count_ == 0)
        {
            return 0;
        }

        // Copy the currently valid values into a scratch buffer for sorting
        std::array<T, N> temp_values;
        std::copy(values_.begin(), values_.begin() + count_, temp_values.begin());
        std::sort(temp_values.begin(), temp_values.begin() + count_);

        // Calculate the median
        if (count_ % 2 == 0)
        {
            return (temp_values[count_ / 2 - 1] + temp_values[count_ / 2]) / 2;
        }
        else
        {
            return temp_values[count_ / 2];
        }
    }

private:
    size_t count_ = 0;
    size_t index_ = 0;
    T reset_threshold_ = T{};
    bool reset_threshold_set_ = false;
    std::array<T, N> values_{};
};
}
