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

#include "KarplusStrong.hpp"

using namespace badger;

void KarplusStrong::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    delay_line_.fill(0);
    write_index_ = 0u;
    previous_sample_ = 0;
    previous_filtered_ = 0;
    quantization_error_ = 0;

    // Defaults preserve the original Karplus-Strong voicing.
    damping_ = q4_12(0.985f);
    tone_ = q15(0.5f);
    excitation_level_ = q15(0.90f);
    frequency_hz_ = 220.0f;
    tone_low_ = 0.25f;
    tone_high_ = 0.8f;
    tone_comp_range_hz_ = 220.0f;
    tone_comp_depth_ = 0.2f;
    SetFrequency(110.0f);
}

void KarplusStrong::SetFrequency(float frequency_hz)
{
    if (sample_rate_ <= 0.0f)
    {
        return;
    }

    const float min_frequency = sample_rate_ / static_cast<float>(kMaxDelaySamples);
    if (frequency_hz < min_frequency)
    {
        frequency_hz = min_frequency;
    }

    const float max_frequency = sample_rate_ * 0.5f;
    if (frequency_hz > max_frequency)
    {
        frequency_hz = max_frequency;
    }

    size_t delay_samples = static_cast<size_t>((sample_rate_ / frequency_hz) + 0.5f);
    if (delay_samples < 2U)
    {
        delay_samples = 2U;
    }
    else if (delay_samples > kMaxDelaySamples)
    {
        delay_samples = kMaxDelaySamples;
    }

    delay_samples_ = delay_samples;
    frequency_hz_ = frequency_hz;

    const float nyquist = sample_rate_ * 0.5f;
    if (nyquist > 0.0f)
    {
        const float freq_norm = constrain(frequency_hz_ / nyquist, 0.0f, 1.0f);
        float low_factor = 1.0f - freq_norm;
        low_factor *= low_factor;
        const float comp_gain = 1.0f + ((kLossCompMaxGain - 1.0f) * low_factor);
        loss_compensation_ = float_to_q4_12(comp_gain);
    }

    UpdateToneFromFrequency();
}

void KarplusStrong::Trigger()
{
    for (size_t i = 0u; i < delay_samples_; ++i)
    {
        const q15_t noise = white_noise_.ProcessQ15();
        const q15_t excited = q15_mult(noise, excitation_level_);
        delay_line_[i] = static_cast<int8_t>(q15_saturate(excited) >> 8);
    }

    for (size_t i = delay_samples_; i < kMaxDelaySamples; ++i)
    {
        delay_line_[i] = 0;
    }

    write_index_ = 0u;
    previous_sample_ = 0;
    previous_filtered_ = 0;
    quantization_error_ = 0;
}

void KarplusStrong::SetDamping(q15_t damping)
{
    damping_ = damping;
}

void KarplusStrong::SetExcitationLevel(q15_t level)
{
    excitation_level_ = q15_saturate(level);
}

void KarplusStrong::SetToneTracking(float tone_low, float tone_high)
{
    tone_low_ = constrain(tone_low, 0.0f, 1.0f);
    tone_high_ = constrain(tone_high, 0.0f, 1.0f);
    if (tone_high_ < tone_low_)
    {
        const float temp = tone_low_;
        tone_low_ = tone_high_;
        tone_high_ = temp;
    }

    UpdateToneFromFrequency();
}

void KarplusStrong::SetToneCompensation(float range_hz, float depth)
{
    tone_comp_range_hz_ = range_hz > 1.0f ? range_hz : 1.0f;
    tone_comp_depth_ = constrain(depth, 0.0f, 1.0f);
    UpdateToneFromFrequency();
}

void KarplusStrong::UpdateToneFromFrequency()
{
    const float nyquist = sample_rate_ * 0.5f;
    if (nyquist <= 0.0f)
    {
        return;
    }

    const float freq_norm = constrain(frequency_hz_ / nyquist, 0.0f, 1.0f);
    const float base_tone = tone_low_ + (freq_norm * (tone_high_ - tone_low_));

    float low_comp = 1.0f - constrain(frequency_hz_ / tone_comp_range_hz_, 0.0f, 1.0f);
    low_comp *= low_comp;

    const float compensated_tone = constrain(base_tone - (tone_comp_depth_ * low_comp), kToneCompMin, kToneCompMax);
    tone_ = float_to_q15(compensated_tone);
}

q15_t KarplusStrong::Process()
{
    q15_t current_sample = static_cast<q15_t>(delay_line_[write_index_]) << 8;

    const q15_t averaged = (current_sample + previous_sample_) >> 1;
    const q15_t smoothed = q15_add(q15_mult(averaged, tone_), q15_mult(previous_filtered_, q15_inv(tone_)));
    q15_t filtered = q15_mult(smoothed, damping_);
    filtered = q15_mult_q4_12(filtered, loss_compensation_);

    const q15_t shaped = q15_saturate(static_cast<int32_t>(filtered) + static_cast<int32_t>(quantization_error_));
    const int8_t quantized = static_cast<int8_t>(shaped >> 8);
    delay_line_[write_index_] = quantized;
    const q15_t reconstructed = static_cast<q15_t>(quantized) << 8;
    quantization_error_ = q15_sub(shaped, reconstructed);

    write_index_ = (write_index_ + 1U);
    if (write_index_ >= delay_samples_)
    {
        write_index_ = 0u;
    }

    previous_sample_ = current_sample;
    previous_filtered_ = smoothed;

    // gain at the end before returning to match other synthesis methods
    return q15_mult_q4_12(smoothed, kEndGain);
}
