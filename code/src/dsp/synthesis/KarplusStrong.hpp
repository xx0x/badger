/*
MIT License

Copyright (c) 2026 Vaclav Mach (Bastl Instruments)

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
#include <type_traits>

#include "WhiteNoise.hpp"
#include "dsp/math/qmath.hpp"

namespace badger
{

/**
 * @class KarplusStrong
 * @ingroup dsp_synthesis
 * @brief Very lightweight lo-fi Karplus-Strong voice using Q15 processing.
 */
class KarplusStrong
{
public:
    void Init(float sample_rate);
    void SetFrequency(float frequency_hz);
    void Trigger();
    void SetDamping(q15_t damping);
    void SetExcitationLevel(q15_t level);
    void SetToneTracking(float tone_low, float tone_high);
    void SetToneCompensation(float range_hz, float depth);
    q15_t Process();

private:
    static constexpr size_t kMaxDelaySamples = 1024u;
    std::array<int8_t, kMaxDelaySamples> delay_line_{};
    float sample_rate_ = 0.0f;
    size_t delay_samples_ = 2U;
    size_t write_index_ = 0u;
    q15_t previous_sample_ = 0;
    q15_t previous_filtered_ = 0;
    q15_t quantization_error_ = 0;
    q15_t damping_ = q15(0.985f);
    q4_12_t loss_compensation_ = q4_12(1.0f);
    q15_t tone_ = q15(0.5f);
    q15_t excitation_level_ = q15(0.70f);
    float frequency_hz_ = 110.0f;
    float tone_low_ = 0.3f;
    float tone_high_ = 0.9f;
    float tone_comp_range_hz_ = 220.0f;
    float tone_comp_depth_ = 0.4f;
    static constexpr float kToneCompMin = 0.02f;
    static constexpr float kToneCompMax = 0.98f;
    static constexpr float kLossCompMaxGain = 1.04f;
    WhiteNoise white_noise_;

    void UpdateToneFromFrequency();

    // Amplifies the output at the end of processing to match levels of other synthesis methods.
    // Stored as the reciprocal of the original 0.33 divisor, because a multiply is much cheaper than a division on the M0+.
    static constexpr q4_12_t kEndGain = q4_12(1.0f / 0.33f);
};

} // namespace badger
