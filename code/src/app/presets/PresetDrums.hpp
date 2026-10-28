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
 * @brief Two-voice drum preset (kick + snare/noise). Instant party.
 * @author Vaclav Mach
 * @date 2026-06-13
 */
class PresetDrums final : public Preset
{
public:
    struct Specs
    {
        Oscillator::Waveform kick_waveform;
        int32_t kick_drive; // 1 = no drive
        q15_t kick_volume;
        size_t snare_downsample; // 0 = no downsampling
        q15_t snare_volume;
        MapDef<float, 4> decay_map_kick;
        MapDef<float, 4> decay_map_snare;
    };

    static constexpr MapDef<float, 4> kKickDecayMap = {{pot(0.0f), pot(0.5f), pot(0.8f), pot(1.0f)},
                                                       {0.1f, 0.6f, 1.0f, 2.5f}};

    static constexpr MapDef<float, 4> kSnareDecayMap = {{pot(0.0f), pot(0.5f), pot(0.8f), pot(1.0f)},
                                                        {0.05f, 0.2f, 0.5f, 1.0f}};

    static constexpr auto kDistorsionMap = MapDef<int32_t, 4>{
        {pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {q15(0.0f), q15(0.125f), q15(0.5f), q15(1.0f)}};

    /**
     * @brief Traditional-ish kick/snare combo.
     */
    static constexpr Specs kSpecsClassic = {
        .kick_waveform = Oscillator::Waveform::SINE,
        .kick_drive = 2,
        .kick_volume = q15(0.9f),
        .snare_downsample = 0,
        .snare_volume = q15(1.0f),
        .decay_map_kick = kKickDecayMap,
        .decay_map_snare = kSnareDecayMap};

    /**
     * @brief Lo-fi 8-bit drums with downsampled snare.
     */
    static constexpr Specs kSpecs8Bit = {
        .kick_waveform = Oscillator::Waveform::SQUARE,
        .kick_drive = 1,
        .kick_volume = q15(0.7f),
        .snare_downsample = 3,
        .snare_volume = q15(1.0f),
        .decay_map_kick = kKickDecayMap,
        .decay_map_snare = kSnareDecayMap};

    /**
     * @brief Aggressive laser-like pitch-modulated kick and tight snare.
     */
    static constexpr Specs kSpecsLaser = {
        .kick_waveform = Oscillator::Waveform::TRI,
        .kick_drive = 1,
        .kick_volume = q15(0.95f),
        .snare_downsample = 2,
        .snare_volume = q15(0.85f),
        .decay_map_kick = {{pot(0.0f), pot(0.5f), pot(0.8f), pot(1.0f)},
                           {0.04f, 0.5f, 1.5f, 3.0f}},
        .decay_map_snare = {{pot(0.0f), pot(0.5f), pot(0.8f), pot(1.0f)},
                            {0.03f, 0.08f, 0.16f, 0.3f}}};

    PresetDrums(const Config &config, const Specs &specs) : Preset(config), specs_(specs) {}
    PresetDrums(const Config &config) = delete;

    static constexpr float kFilterMin = 2000.0F;
    static constexpr float kFilterMax = 8000.0F;

    static constexpr auto DRUM_ENV = Env::FIRST;
    static constexpr auto DRUM_MOD_ENV = Env::SECOND;
    static constexpr auto NOISE_ENV = Env::THIRD;

    static constexpr auto DRUM_OSC = Osc::FIRST;
    static constexpr auto NOISE_OSC = Osc::THIRD;

    static constexpr auto FILTER = Filter::FIRST;

    /**
     * @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::GAMMA] = pot(0.7f);
    }

    // We cannot update all parameters in one go, because some of them are expensive to compute
    static constexpr uint32_t kParamsUpdateCounterMax = 3;

    /**
     * @brief Updates parameters.
     * @param params Current parameters (UI, MIDI, sequencer...)
     */
    void UpdateParams(const Params &params) override
    {
        auto &dsp = config_.dsp;

        if (params.triggers[TrackSequencer::Track::PRIMARY])
        {
            current_note_ = static_cast<float>(params.notes[TrackSequencer::Track::PRIMARY] + config_.octave_offset * 12);
            dsp.envelopes[DRUM_ENV].Trigger();
            dsp.envelopes[DRUM_MOD_ENV].Trigger();
        }

        if (params.incoming_midi_note.note_on)
        {
            current_note_ = static_cast<float>(params.incoming_midi_note.note);
            dsp.envelopes[DRUM_ENV].Trigger();
            dsp.envelopes[DRUM_MOD_ENV].Trigger();
        }

        const float midi_pitch_hz = midi_note_to_freq(current_note_ + params.pitch_bend);
        kick_base_native_freq_ = dsp.oscillators[DRUM_OSC].ToNativeFrequency(midi_pitch_hz);

        if (params.triggers[TrackSequencer::Track::SECONDARY])
        {
            snare_length_mod_ = map(params.notes[TrackSequencer::Track::SECONDARY] % 12, 0, 12, 0, 0.6f);
            dsp.envelopes[NOISE_ENV].Trigger();
        }

        // The pitch sweep itself is applied per block in AudioLoop(), right after its envelope advances
        kick_mod_depth_ = pot_to_q15(preset_pots_[Pot::ALPHA]);

        if (params_update_counter_ == 0)
        {
            float filter_hz = map(preset_pots_[Pot::GAMMA], 0, AnalogInputs::kMaxValue, kFilterMin, kFilterMax);
            dsp.filters[FILTER].SetFrequency(filter_hz);
        }

        if (params_update_counter_ == 1)
        {
            float decay_time_kick = curve_map(params.global_pots[Pot::BETA], specs_.decay_map_kick, MapClamp::TRUE, MapSafe::FALSE);
            dsp.envelopes[DRUM_ENV].SetDecayTime(decay_time_kick);
        }

        if (params_update_counter_ == 2)
        {
            float decay_time_snare = curve_map(params.global_pots[Pot::BETA], specs_.decay_map_snare, MapClamp::TRUE, MapSafe::FALSE);
            dsp.envelopes[NOISE_ENV].SetDecayTime(decay_time_snare + snare_length_mod_);
        }

        dsp.distortion.SetDrive(curve_map(preset_pots_[Pot::BETA], kDistorsionMap));
        params_update_counter_ = (params_update_counter_ + 1) % kParamsUpdateCounterMax;
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        auto drum_env = dsp.envelopes[DRUM_ENV].ProcessRamp(output_buffer.size());
        auto noise_env = dsp.envelopes[NOISE_ENV].ProcessRamp(output_buffer.size());

        // Kick pitch sweep, from the envelope of this very block (so a fresh kick starts at the top of the sweep)
        const q15_t drum_mod_env = q31_to_q15(dsp.envelopes[DRUM_MOD_ENV].Process());
        dsp.oscillators[DRUM_OSC].SetNativeFrequency(q31_freq_mod(kick_base_native_freq_, drum_mod_env, kick_mod_depth_, 3, 4));

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            // Kick
            q15_t sample_kick = dsp.oscillators[DRUM_OSC].Process();
            sample_kick = q15_mult(sample_kick, drum_env.Next());
            if (specs_.kick_drive > 1)
            {
                sample_kick *= specs_.kick_drive;
                sample_kick = q15_saturate(sample_kick);
            }
            sample_kick = q15_mult(sample_kick, specs_.kick_volume);

            // Snare
            if (snare_downsample_index_ > specs_.snare_downsample)
            {
                snare_downsample_sample_ = dsp.oscillators[NOISE_OSC].Process();
                snare_downsample_index_ = 0;
            }
            snare_downsample_index_++;
            q15_t sample_snare = snare_downsample_sample_;
            sample_snare = q15_mult(sample_snare, noise_env.Next(), specs_.snare_volume);

            // Sum drums
            q15_t sample = q15_add(sample_kick, sample_snare);

            // Filter
            sample = dsp.filters[FILTER].Process(sample);

            // Bring back gain after filter stage (multiplying by the reciprocal, the M0+ has no hardware divider)
            sample = q15_mult_q4_12(sample, q4_12(1.0f / 0.8f));

            // Distortion
            sample = dsp.distortion.Process(sample);

            // Store
            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;

        dsp.oscillators[DRUM_OSC].SetWaveform(specs_.kick_waveform);
        kick_base_native_freq_ = dsp.oscillators[DRUM_OSC].ToNativeFrequency(1.0f);
        dsp.oscillators[DRUM_OSC].SetNativeFrequency(kick_base_native_freq_);

        dsp.oscillators[NOISE_OSC].SetWaveform(Oscillator::Waveform::NOISE);

        dsp.envelopes[DRUM_ENV].SetAttackTime(0.001f);
        dsp.envelopes[DRUM_ENV].SetHoldTime(0.0f);
        dsp.envelopes[DRUM_ENV].SetDecayTime(0.5f);
        dsp.envelopes[DRUM_ENV].SetHold(false);

        dsp.envelopes[DRUM_MOD_ENV].SetAttackTime(0.001f);
        dsp.envelopes[DRUM_MOD_ENV].SetHoldTime(0.0f);
        dsp.envelopes[DRUM_MOD_ENV].SetDecayTime(0.2f);
        dsp.envelopes[DRUM_MOD_ENV].SetHold(false);

        dsp.envelopes[NOISE_ENV].SetAttackTime(0.001f);
        dsp.envelopes[NOISE_ENV].SetHoldTime(0.0f);
        dsp.envelopes[NOISE_ENV].SetDecayTime(0.2f);
        dsp.envelopes[NOISE_ENV].SetHold(false);

        dsp.filters[FILTER].SetFrequency(800.0f);
        dsp.filters[FILTER].SetResonance(0.4f);
        dsp.filters[FILTER].SetType(Svf::Type::LOWPASS);

        dsp.distortion.SetDrive(0);
        dsp.distortion.SetVolumeCompensation(true);

        snare_downsample_index_ = 0;
        snare_downsample_sample_ = 0;
    }

    /**
     *  @brief Limits randomizer range for drums.
     */
    OctaveSpan GetRandomizerOctaveSpan() const override
    {
        return {-1, 0};
    }

private:
    const Specs &specs_;
    q31_t kick_base_native_freq_ = 0;
    q15_t kick_mod_depth_ = 0;
    float current_note_ = 60.0f;
    float snare_length_mod_ = 0.0f;
    size_t snare_downsample_index_ = 0;
    q15_t snare_downsample_sample_ = 0;

    size_t params_update_counter_ = 0;
};

}