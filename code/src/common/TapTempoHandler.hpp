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
#include "dsp/math/RunningAverage.hpp"

namespace badger
{

/**
 * @class TapTempoHandler
 * @brief Handles tap tempo functionality for clock synchronization
 * @author Vaclav Mach (Bastl Instruments)
 * @date 2025-11-04
 */
class TapTempoHandler
{
public:
    /**
     * @brief Initializes the tap tempo with sample rate
     * @param sample_rate The audio sample rate
     */
    void Init(float sample_rate);

    /**
     * @brief Processes tap tempo logic
     * @return True if tempo was updated from tap
     */
    bool Process();

    /**
     * @brief Adds a tap to the tap tempo calculation
     */
    void TapTempo();

    /**
     * @brief Gets the number of taps recorded for tap tempo
     * @return Number of taps
     */
    size_t GetTapsCount() const { return taps_count_; }

    /**
     * @brief Clears the tap tempo calculation
     */
    void ClearTapTempo();

    /**
     * @brief Gets the calculated BPM from tapping
     * @return BPM calculated from taps, 0 if no valid tempo
     */
    float GetTappedBpm() const { return tapped_bpm_; }

    /**
     * @brief Checks if tapping is active
     * @return True if currently in a tapping session
     */
    bool IsTapping() const { return taps_count_ > 0; }

    // Constants from original Clock class
    static constexpr size_t kTapTempoAveraging = 6;
    static constexpr float kMaxDelayBetweenTaps = 4.0f; // in seconds

private:
    // Tap tempo state
    float sample_rate_ = 0.0f;
    bool tap_flag_ = false;
    size_t tap_tempo_tick_ = 0;
    size_t taps_count_ = 0;
    float tapped_bpm_ = 0.0f;

    // Used to smooth tap tempo timing
    RunningAverage<size_t, kTapTempoAveraging> tap_tempo_ticks_;
};

} // namespace badger