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
#include "dsp/math/math_utils.hpp"
#include "hardware/AnalogInputs.hpp"

namespace badger
{

/**
 * @brief FM synthesis preset with multiple timbral configurations.
 * @author Vaclav Mach
 * @date 2026-06-11
 */
class PresetFm final : public Preset
{
public:
    static constexpr auto kAttackMap = MapDef<float, 3>{
        {pot(0.0f), pot(0.5f), pot(1.0f)},
        {0.05f, 0.3f, 1.0f}};

    static constexpr auto kNoAttackMap = MapDef<float, 3>{
        {pot(0.0f), pot(0.5f), pot(1.0f)},
        {0.001f, 0.001f, 0.001f}};

    static constexpr auto kDecayMap = MapDef<float, 4>{
        {pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.05f, 0.7f, 1.5f, 4.0f}};

    struct Specs
    {
        std::array<Fraction, 5> ratios;
        MapDef<int32_t, 3> index_map;
        float transient_decay;
        q15_t feedback;
        float tone;
        float mod_smoothing;
        MapDef<int32_t, 3> lfo_mod_depth_map;
        bool hold_env;
        Oscillator::Waveform carrier_waveform;
        Oscillator::Waveform modulator_waveform;
        AdsrEnv::NonResetting non_resetting;
        MapDef<float, 3> attack_map;
    };

    /**
     * @brief Piano-like FM voice with moderate index and short attack.
     */
    static constexpr Specs kSpecsPiano = {
        .ratios = {
            Fraction(1, 1),
            Fraction(3, 2),
            Fraction(2, 1),
            Fraction(5, 2),
            Fraction(3, 1),
        },
        .index_map = MapDef<int32_t, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {q15(0.05f), q15(0.11f), q15(0.31f)}},
        .transient_decay = 0.022f,
        .feedback = q15(0.003f),
        .tone = 4200.0f,
        .mod_smoothing = 3800.0f,
        .lfo_mod_depth_map = MapDef<int32_t, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {q15(0.0f), q15(0.3f), q15(0.7f)}},
        .hold_env = false,
        .carrier_waveform = Oscillator::Waveform::SINE,
        .modulator_waveform = Oscillator::Waveform::SINE,
        .non_resetting = AdsrEnv::NonResetting::NONE,
        .attack_map = kNoAttackMap};

    /**
     * @brief Soft digital flute with long attack and holdable envelope.
     */
    static constexpr Specs kSpecsDigiFlute = {
        .ratios = {
            Fraction(1, 1),
            Fraction(4, 3),
            Fraction(2, 1),
            Fraction(3, 2),
            Fraction(5, 2),
        },
        .index_map = MapDef<int32_t, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {q15(0.01f), q15(0.04f), q15(0.15f)}},
        .transient_decay = 0.04f,
        .feedback = q15(0.0f),
        .tone = 9000.0f,
        .mod_smoothing = 1200.0f,
        .lfo_mod_depth_map = MapDef<int32_t, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {q15(0.25f), q15(0.3f), q15(0.8f)}},
        .hold_env = true,
        .carrier_waveform = Oscillator::Waveform::SINE,
        .modulator_waveform = Oscillator::Waveform::SINE,
        .non_resetting = AdsrEnv::NonResetting::ATTACK_AND_DECAY,
        .attack_map = kAttackMap};

    /**
     * @brief Inharmonic bell with triangular operators.
     */
    static constexpr Specs kSpecsBell = {
        .ratios = {
            Fraction(20, 10),
            Fraction(27, 10),
            Fraction(32, 10),
            Fraction(41, 10),
            Fraction(54, 10),
        },
        .index_map = MapDef<int32_t, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {q15(0.11f), q15(0.22f), q15(0.43f)}},
        .transient_decay = 0.02f,
        .feedback = q15(0.0015f),
        .tone = 6200.0f,
        .mod_smoothing = 9000.0f,
        .lfo_mod_depth_map = MapDef<int32_t, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {q15(0.0f), q15(0.1f), q15(0.3f)}},
        .hold_env = false,
        .carrier_waveform = Oscillator::Waveform::TRI,
        .modulator_waveform = Oscillator::Waveform::TRI,
        .non_resetting = AdsrEnv::NonResetting::NONE,
        .attack_map = kNoAttackMap};

    PresetFm(const Config &config, const Specs &specs) : Preset(config), specs_(specs) {}
    PresetFm(const Config &config) = delete;

    static constexpr auto AMP_ENV = Env::FIRST;
    static constexpr auto CUSTOM_LFO_OSC = Osc::FIRST;

    /**
     * @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::ALPHA] = pot(0.5f);
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

        if (params.triggers[TrackSequencer::Track::PRIMARY])
        {
            TriggerNote(params.notes[TrackSequencer::Track::PRIMARY] + config_.octave_offset * 12);
        }
        if (params.incoming_midi_note.note_on)
        {
            if (specs_.hold_env)
            {
                dsp.envelopes[AMP_ENV].SetHold(true);
            }
            TriggerNote(params.incoming_midi_note.note);
        }
        if (params.incoming_midi_note.note_off)
        {
            if (specs_.hold_env)
            {
                dsp.envelopes[AMP_ENV].SetHold(false);
            }
        }

        dsp.fm.SetFrequency(midi_note_to_freq(current_note_ + params.pitch_bend));

        float attack_time = curve_map(params.global_pots[Pot::BETA], specs_.attack_map, MapClamp::TRUE, MapSafe::FALSE);
        float decay_time = curve_map(params.global_pots[Pot::BETA], kDecayMap, MapClamp::TRUE, MapSafe::FALSE);
        dsp.envelopes[AMP_ENV].SetAttackTime(attack_time);
        dsp.envelopes[AMP_ENV].SetDecayTime(decay_time);

        size_t ratio_index = pot_to_range(preset_pots_[Pot::ALPHA], specs_.ratios.size() - 1);
        dsp.fm.SetRatio(specs_.ratios[ratio_index]);

        q15_t index = curve_map(preset_pots_[Pot::BETA], specs_.index_map);
        dsp.fm.SetIndex(index);

        lfo_mod_depth_ = curve_map(preset_pots_[Pot::GAMMA], specs_.lfo_mod_depth_map);
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
            q15_t lfo = dsp.oscillators[CUSTOM_LFO_OSC].Process();
            const q15_t lfo_unipolar = q15_saturate((lfo / 2) + Q15_HALF);
            const q15_t lfo_gain = q15_add(q15_inv(lfo_mod_depth_), q15_mult(lfo_unipolar, lfo_mod_depth_));

            q15_t sample = dsp.fm.Process();
            sample = q15_mult(sample, env.Next());
            sample = q15_mult(sample, lfo_gain);
            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief Only the variant that holds its amp envelope (the digital flute) sustains long enough to shape;
     *        the piano and the bell are struck and decay on their own.
     */
    bool AllowAftertouch() const override
    {
        return specs_.hold_env;
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;
        dsp.fm.SetWaveforms(specs_.carrier_waveform, specs_.modulator_waveform);
        dsp.fm.SetRatio(specs_.ratios[specs_.ratios.size() / 2]);
        dsp.fm.SetIndex(q15(0.05f));
        dsp.fm.SetTransientDecay(specs_.transient_decay);
        dsp.fm.SetFeedback(specs_.feedback);
        dsp.fm.SetTone(specs_.tone);
        dsp.fm.SetModSmoothing(specs_.mod_smoothing);
        dsp.envelopes[AMP_ENV].SetNonResetting(specs_.non_resetting);
        dsp.envelopes[AMP_ENV].SetHoldTime(0.0f);
        dsp.envelopes[AMP_ENV].SetHold(false);
        dsp.oscillators[CUSTOM_LFO_OSC].SetFrequency(4.0f);
        dsp.oscillators[CUSTOM_LFO_OSC].SetWaveform(Oscillator::Waveform::SINE);
        lfo_mod_depth_ = 0;
    }

private:
    const Specs &specs_;
    q15_t lfo_mod_depth_ = 0;
    float current_note_ = 60.0f;

    void TriggerNote(int32_t note)
    {
        auto &dsp = config_.dsp;

        // Reset LFO phase on each note trigger for consistent modulation start
        dsp.oscillators[CUSTOM_LFO_OSC].Reset();
        current_note_ = static_cast<float>(note);
        dsp.envelopes[AMP_ENV].Trigger();
        dsp.fm.Trigger();
    }
};
}