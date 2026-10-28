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
 * @brief Sweet percussion preset blending tonal drum and filtered noise.
 */
class PresetPercussions final : public Preset
{
public:
    /**
     * @brief Selects where sequencer/MIDI note pitch is applied.
     */
    enum class FeedFrequencyInto
    {
        DRUM,
        NOISE,
        DRUM_AND_NOISE
    };

    /**
     * @brief Parameters defining one percussion timbre family.
     */
    struct Specs
    {
        MapDef<float, 4> drum_decay_map;
        MapDef<float, 4> noise_decay_map;
        std::array<float, 5> drum_ratios;
        float drum_pitch_mod_depth;
        size_t noise_triggers;
        size_t noise_trigger_interval;
        q15_t noise_gain;
        MapDef<float, 3> noise_filter_frequency_map;
        MapDef<float, 3> noise_filter_resonance_map;
        FeedFrequencyInto feed_frequency_into;
        bool use_drum;
        int32_t extra_noise_offset;
    };

    /**
     * @brief Tambourine-like preset.
     */
    static constexpr Specs kSpecsTambourine = {
        .drum_decay_map = MapDef<float, 4>{
            {pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)},
            {0.1f, 0.4f, 0.7f, 1.0f}},
        .noise_decay_map = MapDef<float, 4>{{pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)}, {0.3f, 0.4f, 0.6f, 0.8f}},
        .drum_ratios = {0.7f, 1.0f, 1.33f, 2.0f, 2.33f},
        .drum_pitch_mod_depth = 0.5f,
        .noise_triggers = 3,
        .noise_trigger_interval = 20,
        .noise_gain = q15(0.25f),
        .noise_filter_frequency_map = MapDef<float, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {500.0f, 5000.0f, 10000.0f}},
        .noise_filter_resonance_map = MapDef<float, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {0.3f, 0.85f, 0.99f}},
        .feed_frequency_into = FeedFrequencyInto::DRUM,
        .use_drum = true,
        .extra_noise_offset = 0};

    /**
     * @brief Woodblock-like preset.
     */
    static constexpr Specs kSpecsWoodblock = {
        .drum_decay_map = MapDef<float, 4>{{pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)}, {0.07f, 0.22f, 0.40f, 0.6f}},
        .noise_decay_map = MapDef<float, 4>{{pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)}, {0.02f, 0.04f, 0.06f, 0.1f}},
        .drum_ratios = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f},
        .drum_pitch_mod_depth = 0.1f,
        .noise_triggers = 2,
        .noise_trigger_interval = 10,
        .noise_gain = q15(0.75f),
        .noise_filter_frequency_map = MapDef<float, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {200.0f, 2600.0f, 10000.0f}},
        .noise_filter_resonance_map = MapDef<float, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {0.8f, 0.90f, 0.999f}},
        .feed_frequency_into = FeedFrequencyInto::DRUM_AND_NOISE,
        .use_drum = true,
        .extra_noise_offset = 24};

    /**
     * @brief Clap-like preset. 909 wannabe.
     */
    static constexpr Specs kSpecsClap = {
        .drum_decay_map = MapDef<float, 4>{{pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)}, {0.05f, 0.7f, 1.5f, 4.0f}},
        .noise_decay_map = MapDef<float, 4>{{pot(0.0f), pot(0.5f), pot(0.75f), pot(1.0f)}, {0.5f, 0.8f, 1.2f, 1.8f}},
        .drum_ratios = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        .drum_pitch_mod_depth = 0.0f,
        .noise_triggers = 2,
        .noise_trigger_interval = 20,
        .noise_gain = q15(0.8f),
        .noise_filter_frequency_map = MapDef<float, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {200.0f, 2200.0f, 10000.0f}},
        .noise_filter_resonance_map = MapDef<float, 3>{{pot(0.0f), pot(0.5f), pot(1.0f)}, {0.3f, 0.70f, 0.99f}},
        .feed_frequency_into = FeedFrequencyInto::NOISE,
        .use_drum = false,
        .extra_noise_offset = 2};

    static constexpr auto DRUM_ENV = Env::FIRST;
    static constexpr auto DRUM_MOD_ENV = Env::SECOND;
    static constexpr auto NOISE_ENV = Env::THIRD;

    static constexpr auto DRUM_OSC_1 = Osc::FIRST;
    static constexpr auto DRUM_OSC_2 = Osc::SECOND;
    static constexpr auto NOISE_OSC = Osc::THIRD;

    static constexpr auto FILTER = Filter::FIRST;

    PresetPercussions(const Config &config, const Specs &specs) : Preset(config), specs_(specs) {}
    PresetPercussions(const Config &config) = delete;

    /**
     * @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::ALPHA] = pot(0.5f);
        preset_pots_[Pot::BETA] = pot(0.5f);
        preset_pots_[Pot::GAMMA] = pot(0.5f);
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
            TriggerNote(params.incoming_midi_note.note);
        }

        const float note_with_bend = current_note_ + params.pitch_bend;
        if (specs_.feed_frequency_into == FeedFrequencyInto::DRUM || specs_.feed_frequency_into == FeedFrequencyInto::DRUM_AND_NOISE)
        {
            drum_base_freq_ = midi_note_to_freq(note_with_bend);
        }

        if (specs_.use_drum)
        {
            // The pitch sweep itself is applied per block in AudioLoop(), right after its envelope advances
            drum_ratio_ = specs_.drum_ratios[pot_to_range(preset_pots_[Pot::ALPHA], specs_.drum_ratios.size() - 1)];

            const float drum_decay = curve_map(params.global_pots[Pot::BETA], specs_.drum_decay_map);
            dsp.envelopes[DRUM_ENV].SetDecayTime(drum_decay);
        }

        const float noise_decay = curve_map(params.global_pots[Pot::BETA], specs_.noise_decay_map);
        dsp.envelopes[NOISE_ENV].SetDecayTime(noise_decay);

        if (specs_.feed_frequency_into == FeedFrequencyInto::NOISE || specs_.feed_frequency_into == FeedFrequencyInto::DRUM_AND_NOISE)
        {
            dsp.filters[FILTER].SetFrequency(midi_note_to_freq(note_with_bend + static_cast<float>(specs_.extra_noise_offset)));
        }
        else
        {
            const float noise_filter_freq = curve_map(preset_pots_[Pot::BETA], specs_.noise_filter_frequency_map);
            dsp.filters[FILTER].SetFrequency(noise_filter_freq);
        }

        const float noise_filter_resonance = curve_map(preset_pots_[Pot::GAMMA], specs_.noise_filter_resonance_map);
        dsp.filters[FILTER].SetResonance(noise_filter_resonance);
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        FireNoiseTrigger();

        auto &dsp = config_.dsp;

        auto drum_env = dsp.envelopes[DRUM_ENV].ProcessRamp(output_buffer.size());
        if (specs_.use_drum)
        {
            // Drum pitch sweep, from the envelope of this very block (so a fresh hit starts at the top of the sweep)
            const float drum_mod = q31_to_float(dsp.envelopes[DRUM_MOD_ENV].Process()) * drum_base_freq_ * specs_.drum_pitch_mod_depth;
            dsp.oscillators[DRUM_OSC_1].SetFrequency(drum_base_freq_ + drum_mod);
            dsp.oscillators[DRUM_OSC_2].SetFrequency(drum_base_freq_ * drum_ratio_ + drum_mod);
        }

        auto noise_env = dsp.envelopes[NOISE_ENV].ProcessRamp(output_buffer.size());

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            // Advanced every sample, even when the drum is unused, to keep the ramp in step
            const q15_t drum_gain = drum_env.Next();

            q15_t drum_sample = 0;
            if (specs_.use_drum)
            {
                const q15_t drum_sample_1 = q15_mult(dsp.oscillators[DRUM_OSC_1].Process(), q15(0.8f));
                const q15_t drum_sample_2 = q15_mult(dsp.oscillators[DRUM_OSC_2].Process(), q15(0.2f));
                drum_sample = q15_mult(q15_add(drum_sample_1, drum_sample_2), drum_gain);
            }

            q15_t noise_sample = q15_mult(dsp.oscillators[NOISE_OSC].Process(), noise_env.Next(), specs_.noise_gain);
            noise_sample = dsp.filters[FILTER].Process(noise_sample);

            q15_t sample = q15_add(drum_sample, noise_sample);
            output_buffer[i] = {sample, sample};
        }
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;

        dsp.envelopes[DRUM_ENV].SetNonResetting(AdsrEnv::NonResetting::NONE);
        dsp.envelopes[DRUM_ENV].SetAttackTime(0.001f);
        dsp.envelopes[DRUM_ENV].SetHoldTime(0.0f);
        dsp.envelopes[DRUM_ENV].SetHold(false);

        dsp.envelopes[DRUM_MOD_ENV].SetNonResetting(AdsrEnv::NonResetting::NONE);
        dsp.envelopes[DRUM_MOD_ENV].SetAttackTime(0.001f);
        dsp.envelopes[DRUM_MOD_ENV].SetHoldTime(0.0f);
        dsp.envelopes[DRUM_MOD_ENV].SetDecayTime(0.1f);
        dsp.envelopes[DRUM_MOD_ENV].SetHold(false);

        dsp.envelopes[NOISE_ENV].SetNonResetting(AdsrEnv::NonResetting::NONE);
        dsp.envelopes[NOISE_ENV].SetAttackTime(0.001f);
        dsp.envelopes[NOISE_ENV].SetHoldTime(0.0f);
        dsp.envelopes[NOISE_ENV].SetHold(false);

        dsp.oscillators[DRUM_OSC_1].SetFrequency(1.0f);
        dsp.oscillators[DRUM_OSC_1].SetWaveform(Oscillator::Waveform::TRI);
        dsp.oscillators[DRUM_OSC_2].SetFrequency(1.0f);
        dsp.oscillators[DRUM_OSC_2].SetWaveform(Oscillator::Waveform::TRI);

        dsp.oscillators[NOISE_OSC].SetFrequency(1.0f);
        dsp.oscillators[NOISE_OSC].SetWaveform(Oscillator::Waveform::NOISE);

        dsp.filters[FILTER].SetType(Svf::Type::BANDPASS);
        // dsp.filters[FILTER].SetFrequency(specs_.noise_filter_frequency);
        // dsp.filters[FILTER].SetResonance(specs_.noise_filter_resonance);
    }

private:
    const Specs &specs_;
    float current_note_ = 60.0f;
    float drum_base_freq_ = 1.0f;
    float drum_ratio_ = 1.0f;
    size_t remaining_noise_triggers_ = 0;
    size_t noise_trigger_interval_ = 0;

    void TriggerNote(int32_t note)
    {
        auto &dsp = config_.dsp;
        current_note_ = static_cast<float>(note);

        if (specs_.feed_frequency_into == FeedFrequencyInto::DRUM || specs_.feed_frequency_into == FeedFrequencyInto::DRUM_AND_NOISE)
        {
            dsp.envelopes[DRUM_MOD_ENV].Trigger();
        }

        // Drum env is triggered always (it's used for the indication)
        dsp.envelopes[DRUM_ENV].Trigger();

        ScheduleNoiseTriggers(specs_.noise_triggers);
    }

    void ScheduleNoiseTriggers(size_t count)
    {
        remaining_noise_triggers_ = count;
        noise_trigger_interval_ = 0;
    }

    void FireNoiseTrigger()
    {
        auto &dsp = config_.dsp;
        if (remaining_noise_triggers_ > 0)
        {
            if (noise_trigger_interval_ == 0)
            {
                dsp.envelopes[NOISE_ENV].Trigger();
                --remaining_noise_triggers_;
                noise_trigger_interval_ = specs_.noise_trigger_interval;
            }
            else
            {
                --noise_trigger_interval_;
            }
        }
    }
};

} // namespace badger
