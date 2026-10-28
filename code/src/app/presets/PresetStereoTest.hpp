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

#include "Preset.hpp"

namespace badger
{

/**
 * @brief Temporary bench preset for checking out the stereo DAC path.
 * @details
 * Two free-running sine waves, a different one per channel, so the two DAC outputs are easy to
 * tell apart on a scope or by ear: 110 Hz (A2) on the left, 440 Hz (A4) on the right. There is
 * no envelope and no note handling - the tone starts when the preset is entered and simply keeps
 * running, which makes it usable as a steady reference signal.
 * @note Not a musical preset, remove once the new audio system is verified.
 * @author Vaclav Mach
 * @date 2026-08-20
 */
class PresetStereoTest final : public Preset
{
public:
    PresetStereoTest(const Config &config) : Preset(config) {}

    /** @brief Tone on the left channel, low enough to be obviously the "other" one. */
    static constexpr float kLeftFrequencyHz = 110.0f;

    /** @brief Tone on the right channel, two octaves above the left one. */
    static constexpr float kRightFrequencyHz = 110.0f;

    static constexpr auto LEFT_OSC = Osc::FIRST;
    static constexpr auto RIGHT_OSC = Osc::SECOND;

    /**
     * @brief Updates parameters.
     * @param params Current parameters (UI, MIDI, sequencer...)
     * @note Deliberately ignored - the test tones are fixed, so pots, notes and triggers do nothing.
     */
    void UpdateParams(const Params &) override
    {
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; each channel gets its own sine.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            const q15_t left = dsp.oscillators[LEFT_OSC].Process();
            const q15_t right = dsp.oscillators[RIGHT_OSC].Process();
            output_buffer[i] = {left, right};
        }
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;

        dsp.oscillators[LEFT_OSC].SetWaveform(Oscillator::Waveform::SINE);
        dsp.oscillators[LEFT_OSC].SetFrequency(kLeftFrequencyHz);
        dsp.oscillators[LEFT_OSC].Reset();

        dsp.oscillators[RIGHT_OSC].SetWaveform(Oscillator::Waveform::SINE);
        dsp.oscillators[RIGHT_OSC].SetFrequency(kRightFrequencyHz);
        dsp.oscillators[RIGHT_OSC].Reset();
    }

    /**
     * @brief Keeps the LEDs lit, since there is no envelope to animate them from.
     */
    bool GetLedState() const override
    {
        return true;
    }

    /**
     * @brief Returns the current LED brightness as an 8-bit value.
     * @note Always returns full brightness for the stereo test preset.
     */
    uint8_t GetLedBrightness() const override
    {
        return 255u;
    }
};

}
