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

#pragma once

#include <cstdint>
#include "Oscillator.hpp"
#include "WhiteNoise.hpp"
#include "dsp/math/qmath.hpp"
#include "utils/EnumTools.hpp"

namespace badger
{

/**
 * @class MultiOsc
 * @ingroup dsp_synthesis
 * @brief Oscillator that generates all classic waveforms at once.
 * @author Vaclav Mach (Bastl Instruments)
 */
class MultiOsc
{
public:
    using Waveform = Oscillator::Waveform;
    using Output = EnumArray<Waveform, q31_t>;

    void Init(float sample_rate);
    void SetFrequency(float frequency);
    void SetNativeFrequency(q31_t native_frequency);
    void SetTicks(uint32_t ticks);
    uint32_t GetTicks() const;
    uint32_t GetElapsedTicks() const;
    void SetAmplitude(q31_t amplitude);
    void SetPulseWidth(q31_t pulse_width);
    bool IsEOR() const;
    bool IsEOC() const;
    bool IsRising() const;
    bool IsFalling() const;
    const Output &Process();
    const Output &GetOutput() const;
    q31_t GetPhase() const;
    void PhaseAdd(q31_t phase);
    void Reset(q31_t phase = Q31_MIN);
    static q31_t CalcPhaseIncrement(q31_t frequency);

private:
    float sample_rate_ = 0.0f;
    q31_t amplitude_ = 0;
    q31_t phase_ = 0;
    q31_t phase_inc_ = 0;
    q31_t pulse_width_ = 0;
    q31_t native_frequency_ = 0;
    Output out_{};
    bool eor_ = false;
    bool eoc_ = false;
    uint32_t ticks_ = 0;
    uint32_t ticks_counter_ = 0;
    WhiteNoise white_noise_;
};

}