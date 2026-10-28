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
 * @brief Organ/chord preset with selectable triad voicings.
 */
class PresetOrgan final : public Preset
{
public:
    using OscScale = EnumArray<Osc, int8_t>;

    static constexpr float kFilterResonance = 0.2f;
    static constexpr q15_t kOscVolume = q15(1.0f / 3.0f);
    // Applied as a multiply by the reciprocal, the M0+ has no hardware divider
    static constexpr q4_12_t kFilterMakeupGain = q4_12(1.0f / 0.7f);
    static constexpr float kFilterMin = 600.0f;
    static constexpr float kFilterMax = 5000.0f;

    static constexpr auto AMP_ENV = Env::FIRST;

    static constexpr auto FILTER = Filter::FIRST;

    static constexpr auto kAttackMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.05f, 0.1f, 0.1f, 0.2f, 0.35f}};

    static constexpr auto kHoldMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.05f, 0.1f, 0.2f, 0.5f, 1.0f}};

    static constexpr auto kDecayMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.05f, 0.1f, 0.2f, 0.25f, 0.45f}};

    PresetOrgan(const Config &config) : Preset(config) {}

    /**
     * @brief Chord voicing table selected by local ALPHA pot.
     */
    static constexpr std::array<OscScale, 5> kScales = {
        OscScale{0, 4, 7},
        OscScale{0, 3, 8},
        OscScale{0, 12, 7},
        OscScale{0, 12, -12},
        OscScale{0, 24, -12},
    };

    /**
     * @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::BETA] = pot(0.8f);
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
            TriggerNote(params.notes[TrackSequencer::Track::PRIMARY] + config_.octave_offset * 12, params.current_scale);
        }

        if (params.incoming_midi_note.note_on)
        {
            dsp.envelopes[AMP_ENV].SetHold(true);
            TriggerNote(params.incoming_midi_note.note, params.current_scale);
        }

        if (params.incoming_midi_note.note_off)
        {
            dsp.envelopes[AMP_ENV].SetHold(false);
        }

        const float note_with_bend = current_note_ + params.pitch_bend;
        for (auto osc : EnumRange<Osc>())
        {
            int8_t note_offset = kScales[scale_index_][osc];
            note_offset = params.current_scale.ClosestTo(note_offset);

            float midi_pitch_hz = midi_note_to_freq(note_with_bend + static_cast<float>(note_offset));
            dsp.oscillators[osc].SetFrequency(midi_pitch_hz);
        }

        float filter_hz = map(preset_pots_[Pot::BETA], 0, AnalogInputs::kMaxValue, kFilterMin, kFilterMax);
        dsp.filters[FILTER].SetFrequency(filter_hz);

        scale_index_ = map(preset_pots_[Pot::ALPHA], 0, AnalogInputs::kMaxValue + 10, 0, kScales.size());

        float attack_time = curve_map(params.global_pots[Pot::BETA], kAttackMap, MapClamp::FALSE, MapSafe::FALSE);
        float decay_time = curve_map(params.global_pots[Pot::BETA], kDecayMap, MapClamp::FALSE, MapSafe::FALSE);

        dsp.envelopes[AMP_ENV].SetAttackTime(attack_time);
        dsp.envelopes[AMP_ENV].SetDecayTime(decay_time);

        if (params.play_mode == PlayMode::INTERNAL)
        {
            dsp.envelopes[AMP_ENV].SetHoldTime(curve_map(params.global_pots[Pot::BETA], kHoldMap, MapClamp::FALSE, MapSafe::FALSE));
        }
        else
        {
            dsp.envelopes[AMP_ENV].SetHoldTime(0.0f);
        }
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        auto env = dsp.envelopes[AMP_ENV].ProcessRamp(output_buffer.size());

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            q15_t sample = 0;
            for (auto &oscillator : dsp.oscillators)
            {
                sample = q15_add(sample, q15_mult(oscillator.Process(), kOscVolume));
            }
            sample = q15_mult(sample, env.Next());
            sample = dsp.filters[FILTER].Process(sample);
            // Boost the output to compensate for the filter
            sample = q15_mult_q4_12(sample, kFilterMakeupGain);
            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief The organ holds its amp envelope for as long as the note is held, so pressure has a note to work on.
     */
    bool AllowAftertouch() const override
    {
        return true;
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;

        dsp.filters[FILTER].SetType(Svf::Type::LOWPASS);
        dsp.filters[FILTER].SetResonance(kFilterResonance);

        for (auto &oscillator : dsp.oscillators)
        {
            oscillator.SetWaveform(Oscillator::Waveform::SAW);
        }

        dsp.envelopes[AMP_ENV].SetHold(false);
    }

    /**
     *  @brief Limits randomizer range.
     */
    OctaveSpan GetRandomizerOctaveSpan() const override
    {
        return {-1, 1};
    }

private:
    float current_note_ = 60.0f;
    size_t scale_index_ = 0;

    void TriggerNote(int32_t note, const Scale &sequencer_scale)
    {
        auto &dsp = config_.dsp;
        (void)sequencer_scale;
        current_note_ = static_cast<float>(note);
        dsp.envelopes[AMP_ENV].Trigger();
    }
};

}
