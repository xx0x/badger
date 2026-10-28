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
 * @brief Three-oscillator detuned saw preset. Witch house, dark synth.
 * @author Vaclav Mach
 * @date 2026-06-11
 */
class PresetWitchSaw final : public Preset
{
public:
    static constexpr auto kAmpAttackMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.007f, 0.015f, 0.030f, 0.055f, 0.100f}};

    static constexpr auto kAmpDecayMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.110f, 0.260f, 0.480f, 0.870f, 1.500f}};

    static constexpr auto kFilterAttackMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.014f, 0.029f, 0.060f, 0.110f, 0.200f}};

    static constexpr auto kFilterDecayMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.230f, 0.380f, 0.120f, 0.240f, 0.600f}};

    static constexpr auto kHoldMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.070f, 0.190f, 0.460f, 0.730f, 1.000f}};

    static constexpr auto kResonanceMap = MapDef<float, 3>{
        {pot(0.0f), pot(0.5f), pot(1.0f)},
        {0.4f, 0.8f, 0.9f}};

    static constexpr EnumArray<Osc, q15_t> kOscVolumes = {q15(0.4f), q15(0.4f), q15(0.2f)};

    static constexpr auto AMP_ENV = Env::FIRST;
    static constexpr auto FILTER_ENV = Env::SECOND;

    static constexpr auto OSC_MAIN = Osc::FIRST;
    static constexpr auto OSC_DETUNED = Osc::SECOND;
    static constexpr auto OSC_SUB = Osc::THIRD;

    static constexpr auto FILTER = Filter::FIRST;

    // In semitones (0.5f semitones = 50 cents)
    static constexpr float kMaxDetune = 0.5f;

    // 1 octave suboscillator
    static constexpr float kSubOscOffset = -12.0f;

    /** @brief Constructs the WitchSaw preset with shared DSP configuration. */
    PresetWitchSaw(const Config &config) : Preset(config) {}

    /**
     * @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::ALPHA] = pot(0.5f);
        preset_pots_[Pot::BETA] = pot(0.66f);
        preset_pots_[Pot::GAMMA] = pot(0.33f);
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
            const int32_t note = params.notes[TrackSequencer::Track::PRIMARY] + config_.octave_offset * 12;
            TriggerNote(note);
        }

        if (params.incoming_midi_note.note_on)
        {
            dsp.envelopes[AMP_ENV].SetHoldTime(0.0f);
            dsp.envelopes[AMP_ENV].SetHold(true);
            TriggerNote(params.incoming_midi_note.note);
        }

        if (params.incoming_midi_note.note_off)
        {
            dsp.envelopes[AMP_ENV].SetHold(false);
        }

        float detune = map(preset_pots_[Pot::ALPHA], 0, AnalogInputs::kMaxValue, 0, kMaxDetune);
        const float note_with_bend = current_note_ + params.pitch_bend;
        dsp.oscillators[OSC_MAIN].SetFrequency(midi_note_to_freq(note_with_bend));
        dsp.oscillators[OSC_DETUNED].SetFrequency(midi_note_to_freq(note_with_bend + detune));
        dsp.oscillators[OSC_SUB].SetFrequency(midi_note_to_freq(note_with_bend + kSubOscOffset));

        float filter_hz = map(preset_pots_[Pot::BETA], 0, AnalogInputs::kMaxValue, 200, 5000);
        filter_hz += q15_to_float(filter_env_) * 1000.0f;
        dsp.filters[FILTER].SetFrequency(filter_hz);

        float hold_time = curve_map(params.global_pots[Pot::BETA], kHoldMap, MapClamp::TRUE, MapSafe::FALSE);

        float amp_attack_time = curve_map(params.global_pots[Pot::BETA], kAmpAttackMap, MapClamp::TRUE, MapSafe::FALSE);
        dsp.envelopes[AMP_ENV].SetAttackTime(amp_attack_time);
        float amp_decay_time = curve_map(params.global_pots[Pot::BETA], kAmpDecayMap, MapClamp::TRUE, MapSafe::FALSE);

        if (params.play_mode == PlayMode::INTERNAL)
        {
            dsp.envelopes[AMP_ENV].SetHoldTime(hold_time);
            dsp.envelopes[AMP_ENV].SetDecayTime(amp_decay_time);
        }
        else if (params.play_mode == PlayMode::MIDI)
        {
            dsp.envelopes[AMP_ENV].SetHoldTime(0.0f);
            dsp.envelopes[AMP_ENV].SetDecayTime(2.0f * amp_decay_time);
        }

        float filter_attack_time = curve_map(params.global_pots[Pot::BETA], kFilterAttackMap, MapClamp::TRUE, MapSafe::FALSE);
        dsp.envelopes[FILTER_ENV].SetAttackTime(filter_attack_time);
        float filter_decay_time = curve_map(params.global_pots[Pot::BETA], kFilterDecayMap, MapClamp::TRUE, MapSafe::FALSE);
        dsp.envelopes[FILTER_ENV].SetDecayTime(filter_decay_time);

        if (params.play_mode == PlayMode::INTERNAL)
        {
            dsp.envelopes[FILTER_ENV].SetHoldTime(hold_time);
        }
        else if (params.play_mode == PlayMode::MIDI)
        {
            dsp.envelopes[FILTER_ENV].SetHoldTime(0.0f);
        }

        dsp.filters[FILTER].SetResonance(curve_map(preset_pots_[Pot::GAMMA], kResonanceMap, MapClamp::TRUE, MapSafe::FALSE));
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        auto amp_env = dsp.envelopes[AMP_ENV].ProcessRamp(output_buffer.size());
        filter_env_ = q31_to_q15(dsp.envelopes[FILTER_ENV].Process());

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            q15_t sample = 0;
            for (auto osc : EnumRange<Osc>())
            {
                q15_t sample_osc = dsp.oscillators[osc].Process();
                sample = q15_add(sample, q15_mult(sample_osc, kOscVolumes[osc]));
            }
            sample = q15_mult(sample, amp_env.Next());
            sample = dsp.filters[FILTER].Process(sample);
            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief The witch saw holds its amp envelope for as long as the note is held, so pressure has a note to work on.
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

        dsp.envelopes[AMP_ENV].SetNonResetting(AdsrEnv::NonResetting::NONE);
        dsp.envelopes[AMP_ENV].SetHold(false);
        dsp.envelopes[FILTER_ENV].SetNonResetting(AdsrEnv::NonResetting::NONE);
        dsp.envelopes[FILTER_ENV].SetHold(false);

        dsp.oscillators[OSC_MAIN].SetWaveform(Oscillator::Waveform::SAW);
        dsp.oscillators[OSC_DETUNED].SetWaveform(Oscillator::Waveform::SAW);
        dsp.oscillators[OSC_SUB].SetWaveform(Oscillator::Waveform::SQUARE);

        dsp.filters[FILTER].SetType(Svf::Type::LOWPASS);
    }

private:
    float current_note_ = 60.0f;
    q15_t filter_env_ = 0;

    void TriggerNote(int32_t note)
    {
        auto &dsp = config_.dsp;
        current_note_ = static_cast<float>(note);

        for (auto &oscillator : dsp.oscillators)
        {
            oscillator.Reset();
        }
        dsp.envelopes[AMP_ENV].Trigger();
        dsp.envelopes[FILTER_ENV].Trigger();
    }
};

}
