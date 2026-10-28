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
#include "dsp/math/HyperRandomness.hpp"
#include "hardware/AnalogInputs.hpp"

namespace badger
{

/**
 * @brief Plucked-string bass guitar preset based on Karplus-Strong synthesis.
 * @author Vaclav Mach
 * @date 2026-06-11
 */
class PresetBassGuitar final : public Preset
{
public:
    PresetBassGuitar(const Config &config) : Preset(config) {}

    static constexpr MapDef<q15_t, 5> kDampMap = {
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {q15(0.58f), q15(0.84f), q15(0.935f), q15(0.975f), q15(0.998f)}};

    static constexpr auto kDistorsionMap = MapDef<int32_t, 4>{
        {pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {q15(0.0f), q15(0.067f), q15(0.25f), q15(0.5f)}};

    // just for visualization in the UI, not used in the audio path
    static constexpr auto INDICATE_ENV = Env::FIRST;

    /**
     * @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::ALPHA] = pot(0.15f);
        preset_pots_[Pot::BETA] = pot(0.25f);
        preset_pots_[Pot::GAMMA] = pot(0.2f);
    }

    /**
     * @brief Updates parameters.
     * @param params Current parameters (UI, MIDI, sequencer...)
     */
    void UpdateParams(const Params &params) override
    {
        auto &dsp = config_.dsp;

        if (params.triggers[TrackSequencer::Track::PRIMARY])
        {
            // Using secondary track to generate randomness simulating a human playing
            const float randomness_feed = map(positive_modulo(params.notes[TrackSequencer::Track::SECONDARY], 12), 0, 11, 0.0f, 1.0f);

            TriggerNote(params.notes[TrackSequencer::Track::PRIMARY] + config_.octave_offset * 12, params.pitch_bend, randomness_feed);
        }

        if (params.incoming_midi_note.note_on)
        {
            TriggerNote(params.incoming_midi_note.note, params.pitch_bend);
        }

        const float midi_pitch_hz = midi_note_to_freq(current_note_ + params.pitch_bend);
        dsp.karplus_strong.SetFrequency(midi_pitch_hz);

        const q15_t karplus_damping = curve_map(params.global_pots[Pot::BETA], kDampMap, MapClamp::TRUE, MapSafe::FALSE);
        dsp.karplus_strong.SetDamping(karplus_damping);

        const float tone_low_base = map(preset_pots_[Pot::ALPHA], 0, static_cast<float>(AnalogInputs::kMaxValue), 0.0f, 0.45f);
        const float tone_low_additional = map(params.global_pots[Pot::BETA], 0, static_cast<float>(AnalogInputs::kMaxValue), 0.07f, 0.22f);

        dsp.karplus_strong.SetToneTracking(tone_low_base + tone_low_additional + tone_low_randomness_, 0.9f);

        q15_t little_of_drive = curve_map(preset_pots_[Pot::BETA], kDistorsionMap);
        dsp.distortion.SetDrive(little_of_drive);
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        dsp.envelopes[INDICATE_ENV].Process();

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            q15_t sample = dsp.karplus_strong.Process();
            sample = dsp.distortion.Process(sample);
            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;

        dsp.envelopes[INDICATE_ENV].SetAttackTime(0.001f);
        dsp.envelopes[INDICATE_ENV].SetHoldTime(0.0f);
        dsp.envelopes[INDICATE_ENV].SetDecayTime(.4f);
        dsp.envelopes[INDICATE_ENV].SetHold(false);

        dsp.distortion.SetVolumeCompensation(true);
        dsp.distortion.SetDrive(0);
    }

    /**
     * @brief Constrains octave randomization range for the bass.
     */
    OctaveSpan GetRandomizerOctaveSpan() const override
    {
        return {-1, 1};
    }

private:
    float tone_low_randomness_ = 0.0f;
    float current_note_ = 60.0f;

    void TriggerNote(int32_t note, float pitch_bend, float randomness_feed = -1.0f)
    {
        auto &dsp = config_.dsp;
        current_note_ = static_cast<float>(note);

        // Tune to the new note before plucking: Trigger() only excites the delay line as long as it currently is,
        // so a note lower than the previous one would otherwise get only part of its longer loop filled
        dsp.karplus_strong.SetFrequency(midi_note_to_freq(current_note_ + pitch_bend));
        dsp.karplus_strong.Trigger();
        dsp.envelopes[INDICATE_ENV].Trigger();

        // If we don't have a randomness feed from the secondary track, generate a true random value
        if (randomness_feed < 0.0f)
        {
            randomness_feed = HyperRandomness::NextFloat();
        }

        // Apply the randomness to the tone
        tone_low_randomness_ = map(preset_pots_[Pot::GAMMA], 0, static_cast<float>(AnalogInputs::kMaxValue), 0.0f, 0.3f) * randomness_feed;
    }
};

}
