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

#include "LoFiFm.hpp"

using namespace badger;

void LoFiFm::Init(float sample_rate)
{
    sample_rate_ = sample_rate;

    car_.Init(sample_rate_);
    mod_.Init(sample_rate_);

    car_.SetWaveform(Oscillator::Waveform::SINE);
    mod_.SetWaveform(Oscillator::Waveform::SINE);

    SetFrequency(220.0f);
    SetRatio(Fraction(1, 1));
    SetIndex(q15(0.06f));
    SetTransientDecay(0.02f);
    SetFeedback(q15(0.006f));
    SetModSmoothing(6000.0f);
    SetTone(2200.0f);

    Reset();
}

q15_t LoFiFm::Process()
{
    transient_env_ = q15_mult(transient_env_, transient_decay_coef_);
    const q15_t dynamic_index = q15_add(index_base_, q15_mult(index_transient_, transient_env_));

    const q15_t mod_raw = mod_.Process();
    const q15_t mod_delta = q15_sub(mod_raw, mod_smooth_);
    mod_smooth_ = q15_add(mod_smooth_, q15_mult(mod_delta, mod_smooth_alpha_));

    q15_t phase_mod = q15_mult(mod_smooth_, dynamic_index);
    phase_mod = q15_add(phase_mod, q15_mult(prev_car_, feedback_));
    car_.PhaseAdd(phase_mod);

    const q15_t raw = car_.Process();
    prev_car_ = raw;

    const q15_t tone_delta = q15_sub(raw, tone_state_);
    tone_state_ = q15_add(tone_state_, q15_mult(tone_delta, tone_alpha_));

    return tone_state_;
}

void LoFiFm::SetFrequency(float frequency)
{
    SetNativeFrequency(OscillatorQ15::ToNativeFrequency(frequency, sample_rate_));
}

void LoFiFm::SetNativeFrequency(q31_t freq)
{
    freq_ = freq;
    car_.SetNativeFrequency(freq_);
    UpdateModulatorFrequency();
}

void LoFiFm::UpdateModulatorFrequency()
{
    // 64 bits, a q31 frequency times the ratio numerator doesn't fit into 32
    int64_t mod_freq = static_cast<int64_t>(freq_) * static_cast<int64_t>(ratio_.n);
    mod_freq = (mod_freq + (ratio_.d / 2)) / ratio_.d;
    if (mod_freq < 0)
    {
        mod_freq = 0;
    }
    else if (mod_freq > Q31_MAX)
    {
        mod_freq = Q31_MAX;
    }
    mod_.SetNativeFrequency(static_cast<q31_t>(mod_freq));
}

void LoFiFm::SetRatio(const Fraction &ratio)
{
    ratio_ = ratio.Simplify();
    if (ratio_.d == 0)
    {
        ratio_ = Fraction(1, 1);
    }

    if (ratio_.d < 0)
    {
        ratio_.n = -ratio_.n;
        ratio_.d = -ratio_.d;
    }

    if (ratio_.n <= 0)
    {
        ratio_ = Fraction(1, 1);
    }

    UpdateModulatorFrequency();
}

void LoFiFm::SetIndex(q15_t index)
{
    index_total_ = index;
    if (index_total_ < 0)
    {
        index_total_ = 0;
    }
    index_transient_ = q15_mult(index_total_, kDefaultTransientMix);
    index_base_ = q15_sub(index_total_, index_transient_);
}

q15_t LoFiFm::GetIndex() const
{
    return index_total_;
}

void LoFiFm::Trigger()
{
    transient_env_ = q15(1.0f);
}

void LoFiFm::SetTone(float cutoff_hz)
{
    tone_alpha_ = CalcOnePoleAlpha(cutoff_hz, sample_rate_);
}

void LoFiFm::SetTransientDecay(float decay_seconds)
{
    const float safe_decay = decay_seconds < kMinTransientS ? kMinTransientS : decay_seconds;
    const float step = 1.0f / (safe_decay * sample_rate_);
    const float coef = constrain(1.0f - step, 0.0f, 1.0f);
    transient_decay_coef_ = float_to_q15(coef);
}

void LoFiFm::SetFeedback(q15_t amount)
{
    static constexpr q15_t kMaxFeedback = q15(0.04f);
    if (amount < 0)
    {
        feedback_ = 0;
    }
    else if (amount > kMaxFeedback)
    {
        feedback_ = kMaxFeedback;
    }
    else
    {
        feedback_ = amount;
    }
}

void LoFiFm::SetModSmoothing(float cutoff_hz)
{
    mod_smooth_alpha_ = CalcOnePoleAlpha(cutoff_hz, sample_rate_);
}

void LoFiFm::Reset()
{
    car_.Reset();
    mod_.Reset();
    prev_car_ = 0;
    mod_smooth_ = 0;
    tone_state_ = 0;
    transient_env_ = 0;
}

q15_t LoFiFm::CalcOnePoleAlpha(float cutoff_hz, float sample_rate)
{
    const float nyquist = sample_rate * 0.5f;
    float safe_cutoff = cutoff_hz;
    if (safe_cutoff < kMinCutoffHz)
    {
        safe_cutoff = kMinCutoffHz;
    }
    else if (safe_cutoff > nyquist)
    {
        safe_cutoff = nyquist;
    }

    // Fast approximation to avoid heavy math calls in embedded builds.
    const float alpha = constrain((safe_cutoff * 6.283185307f) / sample_rate, 0.0f, 1.0f);
    return float_to_q15(alpha);
}