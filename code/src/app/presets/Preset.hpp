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

#include <cstdint>
#include <span>

#include "hardware/AudioInterface.hpp"

#include "dsp/control/AdsrEnv.hpp"
#include "dsp/effects/SimpleHardClipper.hpp"
#include "dsp/filters/Svf.hpp"
#include "dsp/math/qmath.hpp"
#include "dsp/sampling/EightBitSamplePlayer.hpp"
#include "dsp/sequencing/TrackSequencer.hpp"
#include "dsp/synthesis/KarplusStrong.hpp"
#include "dsp/synthesis/LoFiFm.hpp"
#include "dsp/synthesis/MultiOsc.hpp"
#include "dsp/synthesis/OscillatorQ15.hpp"
#include "hardware/AnalogInputs.hpp"
#include "utils/EnumTools.hpp"

namespace badger
{

/**
 * @brief Base interface for all sound presets.
 *
 * Presets share a single DSP backend instance and a shared parameter block
 * managed by Presets/Application. Each derived preset interprets these inputs
 * differently in UpdateParams() and renders one audio block in AudioLoop().
 *
 * @author Vaclav Mach
 * @date 2026-06-11
 */
class Preset
{
public:
    using Pot = AnalogInputs::Pot;
    using LightSensor = AnalogInputs::LightSensor;

    /**
     * @brief Per-preset local pot values (ALPHA/BETA/GAMMA), 12-bit ADC domain.
     */
    using PresetPots = EnumArray<Pot, uint16_t>;

    /**
     * @brief How the preset is triggered: from internal sequencer or from incoming MIDI events.
     */
    enum class PlayMode : uint8_t
    {
        /** @brief Trigger notes from internal sequencer tracks. */
        INTERNAL,
        /** @brief Trigger notes from incoming MIDI events. */
        MIDI
    };

    /**
     * @brief Oscillator indicies.
     */
    enum class Osc : uint8_t
    {
        FIRST,
        SECOND,
        THIRD,
        COUNT
    };

    /**
     * @brief Envelope indicies.
     */
    enum class Env : uint8_t
    {
        FIRST,
        SECOND,
        THIRD,
        COUNT
    };

    /**
     * @brief Filter indicies.
     */
    enum class Filter : uint8_t
    {
        FIRST,
        SECOND,
        COUNT
    };

    /**
     * @brief What should be randomized when the user presses the RANDOMIZE button.
     */
    enum class RandomizeType : uint8_t
    {
        RHYTHM_MELODY,
        RHYTHM,
        MELODY,
        GLOBAL_POTS,
        PATCH_POTS,
        ALL_POTS,
        COUNT
    };

    /**
     * @brief Shared DSP blocks reused by all presets.
     */
    struct Dsp
    {
        EnumArray<Osc, OscillatorQ15> oscillators;
        EnumArray<Env, AdsrEnv> envelopes;
        EnumArray<Filter, Svf> filters;
        MultiOsc lfo;
        LoFiFm fm;
        KarplusStrong karplus_strong;
        SimpleHardClipper distortion;

        /**
         * @brief Initializes the shared DSP objects with safe defaults (doesn't really matter what the values are).
         */
        void Init(float sample_rate, float callback_rate)
        {
            // Every oscillator has its own noise generator. With one shared seed, two oscillators playing NOISE
            // side by side produce the exact same samples, so panning them apart still sounds mono.
            static constexpr EnumArray<Osc, uint32_t> kNoiseSeeds = {1u, 0x9E3779B9u, 0x7F4A7C15u};

            for (auto osc : EnumRange<Osc>())
            {
                auto &oscillator = oscillators[osc];
                oscillator.Init(sample_rate);
                oscillator.SetWaveform(Oscillator::Waveform::SQUARE);
                oscillator.SetFrequency(440.0f);
                oscillator.SetNoiseSeed(kNoiseSeeds[osc]);
            }
            for (auto &envelope : envelopes)
            {
                envelope.Init(callback_rate);
                envelope.SetAttackTime(0.001f);
                envelope.SetDecayTime(1.0f);
                envelope.SetReleaseTime(0.1f);
            }

            for (auto &filter : filters)
            {
                filter.Init(sample_rate);
                filter.SetType(Svf::Type::LOWPASS);
                filter.SetResonance(0.8f);
                filter.SetDrive(0.0f);
                filter.SetFrequency(2000.0f);
            }

            lfo.Init(callback_rate);

            fm.Init(sample_rate);
            fm.SetWaveforms(Oscillator::Waveform::SINE, Oscillator::Waveform::SINE);
            fm.SetRatio(Fraction(1, 1));
            fm.SetIndex(q15(0.05f));
            fm.SetTransientDecay(0.018f);
            fm.SetTone(1800.0f);
            fm.SetModSmoothing(5500.0f);
            fm.SetFeedback(q15(0.004f));

            karplus_strong.Init(sample_rate);

            distortion.Init();
        }
    };

    /**
     * @brief Per-preset immutable metadata and shared DSP reference.
     */
    struct Config
    {
        /** @brief Main preset color shown on LEDs. */
        Color color = Color::BLACK;
        /** @brief Group color (melody/drums/drone) shown on the center LED when active. */
        Color group_color = Color::BLACK;
        /** @brief Semitone transpose (in octaves) applied by the preset. */
        int32_t octave_offset = 0;
        /** @brief Linear gain compensation applied in the mixer path. */
        q15_t volume_compensation = q15(1.0f);
        /** @brief Shared DSP backend used by all presets. */
        Dsp &dsp;
    };

    /** @brief One-shot MIDI note event bridge from ISR/UI side to preset logic. */
    struct MidiNote
    {
        volatile bool note_on = false;
        volatile bool note_off = false;
        volatile uint8_t note = 0;
        volatile uint8_t velocity = 0;
    };

    /** @brief Shared runtime inputs consumed by UpdateParams(). */
    struct Params
    {
        /** @brief Trigger flags for sequencer tracks (one UI frame long). */
        EnumArray<TrackSequencer::Track, bool> triggers;
        /** @brief Sequencer notes per track, in MIDI note numbers. */
        EnumArray<TrackSequencer::Track, int32_t> notes;
        /** @brief Incoming MIDI note event captured by Application. */
        MidiNote incoming_midi_note;
        /** @brief Combined pitch offset in semitones from MIDI pitch bend and the CV input (v/oct). */
        float pitch_bend = 0.0f;
        /** @brief Global pots edited in NORMAL layer (ALPHA/BETA/GAMMA). */
        EnumArray<Pot, uint16_t> global_pots = {pot(0.5f), pot(0.5f), pot(0.5f)};
        /** @brief Calibrated light sensor values normalized into -1.0 to 1.0 range.*/
        EnumArray<LightSensor, q15_t> light_sensors = {0, 0, 0};
        /** @brief Current base note from sequencer/root/scale state. */
        int32_t base_note = 0;
        /** @brief Current musical scale selected in the sequencer. */
        Scale current_scale;
        /** @brief Internal or MIDI play source selection. */
        PlayMode play_mode = PlayMode::INTERNAL;
    };

    /** @brief Inclusive octave range used when melody randomization is requested. */
    struct OctaveSpan
    {
        int32_t lower = 0;
        int32_t upper = 0;
    };

    /** @brief Constructs a preset with immutable metadata and shared DSP reference. */
    Preset(const Config &config) : config_(config)
    {
    }

    /** @brief One-time initialization hook for per-preset default local pot positions. */
    virtual void Init() {};

    /** @brief Updates preset state from shared UI/MIDI/sequencer parameters. */
    virtual void UpdateParams(const Params &params) = 0;

    /** @brief Renders one block of stereo frames into output_buffer. */
    virtual void AudioLoop(AudioInterface::Buffer output_buffer) = 0;

    /** @brief Called when this preset becomes active (preset switch). */
    virtual void Enter() = 0;

    /**
     * @brief Returns dynamic activity state used for NORMAL-layer LED animation.
     *
     * Default implementation considers the first envelope active above a fixed
     * threshold. Presets can override with custom logic.
     */
    virtual bool GetLedState() const
    {
        return config_.dsp.envelopes[Env::FIRST].GetOutput() > q31(0.5f);
    }

    /**
     * @brief Returns the current LED brightness as an 8-bit value.
     * @note The value is derived from the first envelope output, scaled down to 8 bits.
     */
    virtual uint8_t GetLedBrightness() const
    {
        return config_.dsp.envelopes[Env::FIRST].GetOutput() >> 23;
    }

    /**
     * @brief Whether incoming MIDI aftertouch should modulate this preset's output level.
     * @details Only meaningful for presets that keep sounding for as long as the note is held.
     *          A single-shot preset has already decayed by the time pressure builds up, so
     *          aftertouch would just tug at its release tail. Note-on velocity is unaffected
     *          and applies to every preset either way.
     * @return False by default, overridden by the presets whose note is held long enough to shape.
     */
    virtual bool AllowAftertouch() const
    {
        return false;
    }

    /**
     * @brief Selects what Application::Randomize() should randomize.
     * @return Randomization scope for the currently selected preset.
     */
    virtual RandomizeType GetRandomizeType() const
    {
        return RandomizeType::RHYTHM_MELODY;
    }

    /** @brief Octave limits used when melody randomization is enabled. */
    virtual OctaveSpan GetRandomizerOctaveSpan() const
    {
        return {-2, 1};
    }

    /** @brief Returns this preset's primary UI color. */
    Color GetColor() const
    {
        return config_.color;
    }

    /** @brief Returns this preset's group color. */
    Color GetGroupColor() const
    {
        return config_.group_color;
    }

    /** @brief Returns per-preset gain compensation used by the application mixer. */
    q15_t GetVolumeCompensation() const
    {
        return config_.volume_compensation;
    }

    /**
     * @brief Updates local preset pots selectively.
     * @param pots Optional new values per ALPHA/BETA/GAMMA pot. Missing entries
     * are ignored and keep previous values.
     */
    void UpdatePresetPots(EnumArray<Pot, std::optional<uint16_t>> pots)
    {
        for (auto pot : EnumRange<Pot>())
        {
            if (pots[pot].has_value())
            {
                preset_pots_[pot] = pots[pot].value();
            }
        }
    }

    /** @brief Replaces all local preset pots at once. */
    void SetPresetPots(const PresetPots &pots)
    {
        preset_pots_ = pots;
    }

    /** @brief Sets one local preset pot directly. */
    void SetPresetPot(Pot pot, uint16_t value)
    {
        preset_pots_[pot] = value;
    }

    /** @brief Returns current local preset pots (ALPHA/BETA/GAMMA). */
    PresetPots GetPresetPots() const
    {
        return preset_pots_;
    }

protected:
    /**
     * @brief Immutable preset metadata and reference to shared DSP.
     */
    Config config_;

    /**
     * @brief Local per-preset pot state edited in Y layer and persisted in save file.
     */
    PresetPots preset_pots_{pot(0.0f), pot(0.0f), pot(0.0f)};
};

}