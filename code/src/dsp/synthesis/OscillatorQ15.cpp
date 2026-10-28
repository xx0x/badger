/*
MIT License

Copyright (c) 2020 Electrosmith, Corp
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

#include "OscillatorQ15.hpp"
#include <algorithm>
#include "dsp/math/math_utils.hpp"

using namespace badger;

void OscillatorQ15::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    amplitude_ = Q15_MAX;
    phase_ = Q31_MIN;
    waveform_ = Oscillator::Waveform::SINE;
    pulse_width_ = Q15_ZERO; // 50% duty cycle
    eoc_ = true;
    eor_ = true;
    SetFrequency(100); // 100 Hz
}

void OscillatorQ15::SetFrequency(const float frequency)
{
    SetNativeFrequency(ToNativeFrequency(frequency));
}

void OscillatorQ15::SetNativeFrequency(const q31_t native_frequency)
{
    native_frequency_ = native_frequency;
    phase_inc_ = CalcPhaseIncrement(native_frequency_);
}

void OscillatorQ15::SetTicks(const uint32_t ticks)
{
    // One period spans the full 2 * Q31_MAX phase range, similar to CalcPhaseIncrement()
    phase_inc_ = static_cast<q31_t>(static_cast<uint32_t>(Q31_MAX) / std::max<uint32_t>(ticks / 2U, 1U));
}

void OscillatorQ15::SetAmplitude(const q15_t amplitude)
{
    amplitude_ = amplitude;
}

void OscillatorQ15::SetWaveform(const Oscillator::Waveform waveform)
{
    waveform_ = waveform;
}

void OscillatorQ15::SetPulseWidth(q15_t pulse_width)
{
    pulse_width_ = pulse_width;
}

void OscillatorQ15::SetNoiseSeed(uint32_t seed)
{
    white_noise_.Seed(seed);
}

bool OscillatorQ15::IsEOR()
{
    return eor_;
}

bool OscillatorQ15::IsEOC()
{
    return eoc_;
}

bool OscillatorQ15::IsRising()
{
    return phase_ < Q31_ZERO;
}

bool OscillatorQ15::IsFalling()
{
    return phase_ >= Q31_ZERO;
}

void OscillatorQ15::PhaseAdd(q15_t phase)
{
    // Wraps around within the 32-bit phase, so the waveform is always looked up with an in-range phase
    phase_ = q31_add_wrap(phase_, q15_to_q31(phase));
}

void OscillatorQ15::Reset(q15_t _phase)
{
    phase_ = q15_to_q31(_phase);
}

q15_t OscillatorQ15::Process()
{
    // The waveforms are shaped from the top 16 bits of the phase, the lower bits keep the frequency precise
    const q15_t phase = q31_to_q15(phase_);
    q15_t out;

    switch (waveform_)
    {
    case Oscillator::Waveform::SINE:
        // convert -1:1 range to 0:1 and apply the interpolated sine lookup (with the full phase for a finer interpolation)
        out = q31_to_q15(q31_sine(phase_ / 2 + Q31_HALF));
        break;
    case Oscillator::Waveform::TRI:
        if (phase < Q15_ZERO)
        {
            out = (phase + Q15_HALF) * 2;
        }
        else
        {
            out = (Q15_HALF - phase) * 2;
        }
        break;
    case Oscillator::Waveform::SAW:
        out = -phase;
        break;
    case Oscillator::Waveform::RAMP:
        out = phase;
        break;
    case Oscillator::Waveform::SQUARE:
        out = phase < pulse_width_ ? Q15_MAX : Q15_MIN;
        break;
    case Oscillator::Waveform::NOISE:
        out = white_noise_.ProcessQ15();
        break;
    default:
        out = Q15_ZERO;
        break;
    }

    // Wrapping around is how the next period starts
    const q31_t new_phase = q31_add_wrap(phase_, phase_inc_);

    // overflow
    if (phase_ > new_phase)
    {
        eoc_ = true;
    }
    else
    {
        eoc_ = false;
    }

    // output amplitude
    if (amplitude_ != Q15_MAX)
    {
        out = q15_mult(out, amplitude_);
    }

    // end of rise, complete phase
    eor_ = (phase_ < Q31_ZERO && new_phase >= Q31_ZERO);
    phase_ = new_phase;

    return out;
}

/**
 * @brief Calculates the phase increment for a given relative frequency. Basically multiply by 2, since the Oscillators use -1 to 1 range.
 * @param frequency The frequency to calculate the phase increment for.
 * @return The phase increment for the given frequency.
 */
q31_t OscillatorQ15::CalcPhaseIncrement(q31_t frequency)
{
    int64_t inc = static_cast<int64_t>(frequency) << 1;
    if (inc > Q31_MAX)
    {
        inc = Q31_MAX;
    }
    return static_cast<q31_t>(inc);
}