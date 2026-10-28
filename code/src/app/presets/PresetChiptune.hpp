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
#include "hardware/AnalogInputs.hpp"

namespace badger
{

/**
 * @brief NES in a box. Chiptune-style preset using square leads, sub oscillator, and optional noise.
 * @author Vaclav Mach
 * @date 2026-06-27
 */
class PresetChiptune final : public Preset
{
public:
    static constexpr int32_t kPitchmodOctaveDepthMin = 1;
    static constexpr int32_t kPitchmodOctaveDepthMax = 6;

    static constexpr auto kLengthMap = MapDef<int32_t, 5>{{pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
                                                          {9, 15, 25, 40, 80}};

    static constexpr std::array<int32_t, 5> kSubOscOffset = {-24, -12, 0, 12, 24};

    static constexpr auto kNoiseAmountMap = MapDef<int32_t, 5>{{pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
                                                               {q15(0.0f), q15(0.125f), q15(0.25f), q15(0.5f), q15(1.0f)}};

    static constexpr auto MAIN_OSC = Osc::FIRST;
    static constexpr auto SUB_OSC = Osc::SECOND;
    static constexpr auto NOISE_OSC = Osc::THIRD;

    static constexpr auto MAIN_ENV = Env::FIRST;
    static constexpr auto SUB_ENV = Env::SECOND;

    static constexpr auto FILTER = Filter::FIRST;

    PresetChiptune(const Config &config) : Preset(config) {}

    /**
     *  @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::ALPHA] = pot(0.33f);
        preset_pots_[Pot::BETA] = pot(0.5f);
        preset_pots_[Pot::GAMMA] = pot(0.0f);
    }

    /**
     * @brief Updates parameters.
     * @param params Current parameters (UI, MIDI, sequencer...)
     */
    void UpdateParams(const Params &params) override
    {
        auto &dsp = config_.dsp;

        pitchmod_speed_ = curve_map(params.global_pots[Pot::BETA], kLengthMap, MapClamp::FALSE, MapSafe::FALSE);
        pitchmod_depth_ = map(preset_pots_[Pot::BETA], pot(0.0f), pot(1.0f), kPitchmodOctaveDepthMin, kPitchmodOctaveDepthMax, MapClamp::TRUE, MapSafe::FALSE);

        // Process sequencer source
        if (params.triggers[TrackSequencer::Track::PRIMARY])
        {
            TriggerNote(params.notes[TrackSequencer::Track::PRIMARY] + config_.octave_offset * 12);
        }

        // Process external MIDI source
        if (params.incoming_midi_note.note_on)
        {
            TriggerNote(params.incoming_midi_note.note);
        }

        float final_note = static_cast<float>(current_note_ + pitchmod_octave_ * 12) + params.pitch_bend;
        dsp.oscillators[MAIN_OSC].SetFrequency(midi_note_to_freq(final_note));

        int32_t sub_osc_offset = kSubOscOffset[pot_to_range(preset_pots_[Pot::ALPHA], kSubOscOffset.size() - 1)];
        dsp.oscillators[SUB_OSC].SetFrequency(midi_note_to_freq(static_cast<float>(current_note_ + sub_osc_offset) + params.pitch_bend));

        noise_amount_ = curve_map(preset_pots_[Pot::GAMMA], kNoiseAmountMap, MapClamp::FALSE, MapSafe::FALSE);
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        if (pitchmod_counter_ > 0)
        {
            pitchmod_counter_--;
            if (pitchmod_counter_ == 0)
            {
                if (pitchmod_octave_ != pitchmod_depth_)
                {
                    pitchmod_octave_ += (pitchmod_depth_ > pitchmod_octave_) ? 1 : -1;
                    if (pitchmod_octave_ != pitchmod_depth_)
                    {
                        pitchmod_counter_ = pitchmod_speed_;
                    }
                }
            }
        }

        const bool pitchmod_running = (pitchmod_octave_ != pitchmod_depth_);
        dsp.envelopes[MAIN_ENV].SetHold(pitchmod_running);
        dsp.envelopes[SUB_ENV].SetHold(pitchmod_running);

        auto amp_env_ramp = dsp.envelopes[MAIN_ENV].ProcessRamp(output_buffer.size());
        auto sub_env_ramp = dsp.envelopes[SUB_ENV].ProcessRamp(output_buffer.size());

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            const q15_t amp_env = amp_env_ramp.Next();
            const q15_t sub_env = sub_env_ramp.Next();

            q15_t sample_main = dsp.oscillators[MAIN_OSC].Process();
            sample_main = q15_mult(sample_main, q15(0.7f), amp_env);

            q15_t sample_sub = dsp.oscillators[SUB_OSC].Process();
            sample_sub = q15_mult(sample_sub, q15(0.3f), sub_env);

            q15_t sample_noise = dsp.oscillators[NOISE_OSC].Process();
            sample_noise = q15_mult(sample_noise, noise_amount_, sub_env);

            q15_t sample = q15_add(sample_main, sample_sub, sample_noise);
            sample = dsp.filters[FILTER].Process(sample);

            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;

        dsp.oscillators[MAIN_OSC].SetWaveform(Oscillator::Waveform::SQUARE);
        dsp.oscillators[SUB_OSC].SetWaveform(Oscillator::Waveform::SQUARE);
        dsp.oscillators[NOISE_OSC].SetWaveform(Oscillator::Waveform::NOISE);

        dsp.envelopes[MAIN_ENV].SetHold(false);
        dsp.envelopes[MAIN_ENV].SetHoldTime(0.0f);
        dsp.envelopes[MAIN_ENV].SetAttackTime(0.001f);
        dsp.envelopes[MAIN_ENV].SetDecayTime(0.003f);

        dsp.envelopes[SUB_ENV].SetHold(false);
        dsp.envelopes[SUB_ENV].SetHoldTime(0.0f);
        dsp.envelopes[SUB_ENV].SetAttackTime(0.001f);
        dsp.envelopes[SUB_ENV].SetDecayTime(0.050f);

        // Slight lowpass to round off the edges
        dsp.filters[FILTER].SetType(Svf::Type::LOWPASS);
        dsp.filters[FILTER].SetResonance(0.2f);
        dsp.filters[FILTER].SetDrive(0.0f);
        dsp.filters[FILTER].SetFrequency(4000.0f);
    }

private:
    int32_t current_note_ = 60;

    int32_t pitchmod_depth_ = 0;
    int32_t pitchmod_octave_ = 0;
    size_t pitchmod_counter_ = 0;
    size_t pitchmod_speed_ = 0;

    q15_t noise_amount_ = 0;

    void TriggerNote(int32_t note)
    {
        auto &dsp = config_.dsp;
        current_note_ = note;
        dsp.envelopes[MAIN_ENV].Trigger();
        dsp.envelopes[SUB_ENV].Trigger();
        pitchmod_octave_ = 0;
        pitchmod_counter_ = (pitchmod_depth_ == 0) ? 0 : pitchmod_speed_;
    }
};

}
