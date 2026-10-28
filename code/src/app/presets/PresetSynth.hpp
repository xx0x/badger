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
 * @brief Classic synth preset with filter envelope. Rich acid sound, 303 wannabe.
 * @author Vaclav Mach
 * @date 2026-06-11
 */
class PresetSynth final : public Preset
{
public:
    static constexpr auto kDistorsionMap = MapDef<int32_t, 4>{
        {pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {q15(0.0f), q15(0.25f), q15(0.66f), q15(1.0f)}};

    static constexpr auto kDecayMap = MapDef<float, 4>{
        {pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.05f, 0.7f, 1.5f, 4.0f}};

    PresetSynth(const Config &config) : Preset(config) {}

    static constexpr float kFilterMin = 10.0f;
    static constexpr float kFilterMax = 4000.0f;

    static constexpr auto AMP_ENV = Env::FIRST;
    static constexpr auto FILTER_ENV = Env::SECOND;

    static constexpr auto SQUARE_OSC = Osc::FIRST;
    static constexpr auto SAW_OSC = Osc::SECOND;

    static constexpr auto FILTER = Filter::FIRST;

    /**
     * @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::ALPHA] = pot(0.33f);
        preset_pots_[Pot::BETA] = pot(0.0f);
        preset_pots_[Pot::GAMMA] = pot(0.4f);
    }

    /**
     * @brief Updates parameters.
     * @param params Current parameters (UI, MIDI, sequencer...)
     */
    void UpdateParams(const Params &params) override
    {
        auto &dsp = config_.dsp;

        // Process sequencer source
        if (params.triggers[TrackSequencer::Track::PRIMARY])
        {
            TriggerNote(params.notes[TrackSequencer::Track::PRIMARY] + config_.octave_offset * 12);
        }

        // Process external MIDI source
        if (params.incoming_midi_note.note_on)
        {
            dsp.envelopes[AMP_ENV].SetHold(true);
            TriggerNote(params.incoming_midi_note.note);
        }

        if (params.incoming_midi_note.note_off)
        {
            dsp.envelopes[AMP_ENV].SetHold(false);
        }

        const float midi_pitch_hz = midi_note_to_freq(current_note_ + params.pitch_bend);
        dsp.oscillators[SQUARE_OSC].SetFrequency(midi_pitch_hz);
        dsp.oscillators[SAW_OSC].SetFrequency(midi_pitch_hz);

        float filter_hz = map(params.global_pots[Pot::BETA], 0, AnalogInputs::kMaxValue, kFilterMin, kFilterMax);
        filter_hz += q15_to_float(filter_env_) * 500.0f;
        dsp.filters[FILTER].SetFrequency(filter_hz);

        float decay_time = curve_map(params.global_pots[Pot::BETA], kDecayMap, MapClamp::TRUE, MapSafe::FALSE);
        if (params.play_mode == PlayMode::INTERNAL)
        {
            dsp.envelopes[AMP_ENV].SetDecayTime(decay_time);
            dsp.envelopes[FILTER_ENV].SetDecayTime(decay_time);
        }
        else if (params.play_mode == PlayMode::MIDI)
        {
            dsp.envelopes[AMP_ENV].SetDecayTime(decay_time / 2.0f);    // Shorter decay time for MIDI mode
            dsp.envelopes[FILTER_ENV].SetDecayTime(decay_time / 2.0f); // Shorter decay time for MIDI mode
        }

        static constexpr auto kResonanceMap = MapDef<float, 5>{
            {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
            {0.2f, 0.5f, 0.7f, 0.8f, 0.95f}};
        dsp.filters[FILTER].SetResonance(curve_map(preset_pots_[Pot::GAMMA], kResonanceMap, MapClamp::TRUE, MapSafe::FALSE));

        osc_mix_ = pot_to_q15(preset_pots_[Pot::ALPHA]);

        dsp.distortion.SetDrive(curve_map(preset_pots_[Pot::BETA], kDistorsionMap));
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        filter_env_ = q31_to_q15(dsp.envelopes[FILTER_ENV].Process());

        auto env = dsp.envelopes[AMP_ENV].ProcessRamp(output_buffer.size());
        auto osc_mix_inv = q15_inv(osc_mix_);

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            q15_t sample_square = dsp.oscillators[SQUARE_OSC].Process();
            q15_t sample_saw = dsp.oscillators[SAW_OSC].Process();
            q15_t sample = q15_add(q15_mult(sample_square, osc_mix_inv), q15_mult(sample_saw, osc_mix_));
            sample = q15_mult(sample, env.Next());
            sample = dsp.filters[FILTER].Process(sample);
            sample = dsp.distortion.Process(sample);
            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief The synth holds its amp envelope for as long as the note is held, so pressure has a note to work on.
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

        dsp.oscillators[SQUARE_OSC].SetWaveform(Oscillator::Waveform::SQUARE);
        dsp.oscillators[SAW_OSC].SetWaveform(Oscillator::Waveform::SAW);

        dsp.distortion.SetDrive(0);
        dsp.distortion.SetVolumeCompensation(true);

        dsp.envelopes[AMP_ENV].SetNonResetting(AdsrEnv::NonResetting::NONE);
        dsp.envelopes[AMP_ENV].SetHoldTime(0.0f);
        dsp.envelopes[AMP_ENV].SetHold(false);
        dsp.envelopes[AMP_ENV].SetAttackTime(0.001f);
        dsp.envelopes[AMP_ENV].Reset();

        dsp.envelopes[FILTER_ENV].SetNonResetting(AdsrEnv::NonResetting::NONE);
        dsp.envelopes[FILTER_ENV].SetHoldTime(0.0f);
        dsp.envelopes[FILTER_ENV].SetHold(false);
        dsp.envelopes[FILTER_ENV].SetAttackTime(0.001f);
        dsp.envelopes[FILTER_ENV].SetDecayTime(0.35f);
        dsp.envelopes[FILTER_ENV].Reset();

        filter_env_ = 0;

        dsp.filters[FILTER].SetType(Svf::Type::LOWPASS);
    }

private:
    q15_t osc_mix_ = 0;
    q15_t filter_env_ = 0;
    float current_note_ = 60.0f;

    void TriggerNote(int32_t note)
    {
        auto &dsp = config_.dsp;
        current_note_ = static_cast<float>(note);
        dsp.oscillators[SQUARE_OSC].Reset();
        dsp.oscillators[SAW_OSC].Reset();
        dsp.envelopes[AMP_ENV].Trigger();
        dsp.envelopes[FILTER_ENV].Trigger();
    }
};

}
