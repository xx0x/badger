/*
MIT License

Copyright (c) 2020 Electrosmith, Corp
Copyright (c) 2024 Vaclav Mach (Bastl Instruments)

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

#include "MultiOsc.hpp"
#include <algorithm>
#include "dsp/math/math_utils.hpp"

using namespace badger;

void MultiOsc::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    amplitude_ = Q31_MAX;
    phase_ = Q31_MIN;
    pulse_width_ = Q31_ZERO; // 50% duty cycle
    eoc_ = true;
    eor_ = true;
    SetFrequency(100); // 100 Hz
}

void MultiOsc::SetFrequency(float frequency)
{
    SetNativeFrequency(freq_to_q31(frequency, sample_rate_));
}

void MultiOsc::SetNativeFrequency(q31_t native_frequency)
{
    native_frequency_ = native_frequency;
    phase_inc_ = CalcPhaseIncrement(native_frequency_);
    ticks_ = Q31_MAX / native_frequency_;
}

void MultiOsc::SetTicks(uint32_t ticks)
{
    uint32_t t = ticks;
    if (t == 0)
    {
        t = 1;
    }
    // One period spans the full 2 * Q31_MAX phase range, similar to CalcPhaseIncrement()
    phase_inc_ = static_cast<q31_t>(static_cast<uint32_t>(Q31_MAX) / std::max<uint32_t>(t / 2U, 1U));
}

uint32_t MultiOsc::GetTicks() const
{
    return ticks_;
}

uint32_t MultiOsc::GetElapsedTicks() const
{
    return ticks_counter_;
}

void MultiOsc::SetAmplitude(q31_t amplitude)
{
    amplitude_ = amplitude;
}

void MultiOsc::SetPulseWidth(q31_t pulse_width)
{
    pulse_width_ = pulse_width;
}

bool MultiOsc::IsEOR() const
{
    return eor_;
}

bool MultiOsc::IsEOC() const
{
    return eoc_;
}

bool MultiOsc::IsRising() const
{
    return phase_ < Q31_ZERO;
}

bool MultiOsc::IsFalling() const
{
    return phase_ >= Q31_ZERO;
}

q31_t MultiOsc::GetPhase() const
{
    return phase_;
}

void MultiOsc::PhaseAdd(q31_t phase)
{
    // don't use q31_add to enable overflow
    phase_ = q31_add_wrap(phase_, phase);
}

void MultiOsc::Reset(q31_t phase)
{
    phase_ = phase;
}

const MultiOsc::Output &MultiOsc::Process()
{
    out_[Waveform::SINE] = q31_sine(phase_ / 2 + Q31_HALF);

    if (phase_ < Q31_ZERO)
    {
        out_[Waveform::TRI] = (phase_ + Q31_HALF) * 2;
    }
    else
    {
        out_[Waveform::TRI] = (Q31_HALF - phase_) * 2;
    }

    out_[Waveform::SAW] = -phase_;
    out_[Waveform::RAMP] = phase_;
    out_[Waveform::SQUARE] = phase_ < pulse_width_ ? Q31_MAX : Q31_MIN;
    out_[Waveform::NOISE] = white_noise_.ProcessQ31();

    q31_t new_phase;

    // don't use q31_add to enable overflow
    new_phase = q31_add_wrap(phase_, phase_inc_);
    ticks_counter_++;

    if (phase_ > new_phase)
    {
        eoc_ = true;
        ticks_counter_ = 0;
    }
    else
    {
        eoc_ = false;
    }

    if (amplitude_ != Q31_MAX)
    {
        out_[Waveform::SINE] = q31_mult(out_[Waveform::SINE], amplitude_);
        out_[Waveform::TRI] = q31_mult(out_[Waveform::TRI], amplitude_);
        out_[Waveform::SAW] = q31_mult(out_[Waveform::SAW], amplitude_);
        out_[Waveform::RAMP] = q31_mult(out_[Waveform::RAMP], amplitude_);
        out_[Waveform::SQUARE] = q31_mult(out_[Waveform::SQUARE], amplitude_);
        out_[Waveform::NOISE] = q31_mult(out_[Waveform::NOISE], amplitude_);
    }

    eor_ = (phase_ < Q31_ZERO && new_phase >= Q31_ZERO);
    phase_ = new_phase;

    return out_;
}

const MultiOsc::Output &MultiOsc::GetOutput() const
{
    return out_;
}

q31_t MultiOsc::CalcPhaseIncrement(q31_t frequency)
{
    int64_t inc = static_cast<int64_t>(frequency) << 1;
    if (inc > Q31_MAX)
    {
        inc = Q31_MAX;
    }
    return inc;
}