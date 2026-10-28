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

#include <cstdint>
#include "Oscillator.hpp"
#include "OscillatorQ15.hpp"
#include "dsp/math/Fraction.hpp"
#include "dsp/math/qmath.hpp"

namespace badger
{

/**
 * @class LoFiFm
 * @ingroup dsp_synthesis
 * @brief Two-operator FM voice tuned for smoother lo-fi output.
 */
class LoFiFm
{
public:
    void Init(float sample_rate);

    void SetWaveforms(Oscillator::Waveform carrier_waveform, Oscillator::Waveform modulator_waveform)
    {
        car_.SetWaveform(carrier_waveform);
        mod_.SetWaveform(modulator_waveform);
    }

    q15_t Process();

    void SetFrequency(float frequency);
    void SetNativeFrequency(q31_t freq);
    void SetRatio(const Fraction &ratio);

    /**
     * @brief Sets the overall FM depth (0..1), internally split into static + transient parts.
     */
    void SetIndex(q15_t index);

    q15_t GetIndex() const;

    /**
     * @brief Trigger transient FM depth envelope.
     */
    void Trigger();

    /**
     * @brief Set output tone filter cutoff in Hz.
     */
    void SetTone(float cutoff_hz);

    /**
     * @brief Set transient decay in seconds.
     */
    void SetTransientDecay(float decay_seconds);

    /**
     * @brief Set carrier feedback amount (0..1).
     */
    void SetFeedback(q15_t amount);

    /**
     * @brief Set smoothing amount for the modulator signal in Hz.
     */
    void SetModSmoothing(float cutoff_hz);

    void Reset();

private:
    static constexpr q15_t kDefaultTransientMix = q15(0.35f);

    static constexpr float kMinCutoffHz = 5.0f;
    static constexpr float kMinTransientS = 0.001f;

    float sample_rate_ = 0.0f;

    OscillatorQ15 mod_;
    OscillatorQ15 car_;

    q31_t freq_ = 0;
    Fraction ratio_ = Fraction(1, 1);

    q15_t index_base_ = 0;
    q15_t index_transient_ = 0;
    q15_t index_total_ = 0;

    q15_t transient_env_ = 0;
    q15_t transient_decay_coef_ = Q15_MAX;

    q15_t feedback_ = 0;
    q15_t prev_car_ = 0;

    q15_t mod_smooth_ = 0;
    q15_t mod_smooth_alpha_ = Q15_MAX;

    q15_t tone_state_ = 0;
    q15_t tone_alpha_ = Q15_MAX;

    static q15_t CalcOnePoleAlpha(float cutoff_hz, float sample_rate);

    /**
     * @brief Retunes the modulator to the carrier frequency times the ratio.
     * @details Called from the setters rather than per sample, it needs a division (no hardware divider on the M0+).
     */
    void UpdateModulatorFrequency();
};

} // namespace badger