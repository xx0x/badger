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
#include "utils/EnumBlender.hpp"

namespace badger
{

/**
 * @brief Evolving drone preset with LFO-driven filter or pitch movement.
 *
 * PresetDrone is designed for long-form textures. It uses three oscillators,
 * a blendable multi-waveform LFO, and a low-pass filter. The three local pots
 * are used as follows:
 * - ALPHA: chooses one harmonic offset chord from Specs::offsets
 * - BETA: sets base filter frequency via Specs::filter_map
 * - GAMMA: sets filter resonance via kResonanceMap
 *
 * Global pots shape modulation behavior:
 * - Global ALPHA: LFO frequency via Specs::lfo_freq_map
 * - Global BETA: LFO waveform blend position
 * - Global GAMMA: LFO depth, optionally augmented by light sensors
 *
 * Each kSpecs* profile defines a complete drone identity: oscillator waveforms,
 * harmonic spread table, filter movement range, LFO range, and whether light
 * sensors modulate depth or transposition.
 */
class PresetDrone final : public Preset
{
public:
    /**
     * @brief Primary modulation mode for each profile.
     */
    enum class Flavor
    {
        LFO_FILTER_SWEEP,
        LFO_OFFSET_SWEEP
    };

    /**
     * @brief Selects where light-sensor modulation is routed.
     */
    enum class LightSensorTarget
    {
        LFO_DEPTH,
        OSC_OCTAVES,
        COUNT
    };

    /**
     * @brief Harmonic offset of each oscillator.
     */
    using Offset = EnumArray<Osc, int8_t>;

    /**
     * @brief Gains of one oscillator into the left and right output channel.
     */
    struct OscMix
    {
        q15_t left;
        q15_t right;
    };

    /**
     * @brief Stereo placement and level of each oscillator.
     */
    using StereoMix = EnumArray<Osc, OscMix>;

    /**
     * @brief How many note offsets (ratios) are available for selection.
     */
    static constexpr size_t kNoteOffsets = 7;

    /**
     * @brief Defines the order in which LFO waveforms are blended using the global BETA pot.
     */
    static constexpr std::array<MultiOsc::Waveform, 5> kLfoWaveformBlendOrder = {
        MultiOsc::Waveform::RAMP,
        MultiOsc::Waveform::SQUARE,
        MultiOsc::Waveform::SAW,
        MultiOsc::Waveform::TRI,
        MultiOsc::Waveform::NOISE};

    /**
     * @brief Complete parameter set for one drone flavor.
     */
    struct Specs
    {
        Flavor flavor;
        MapDef<float, 3> filter_map;
        float filter_multiplier;
        int32_t default_filter;
        int32_t default_resonance;
        bool volume_boost;
        EnumArray<Osc, Oscillator::Waveform> waveforms;
        StereoMix stereo_mix;
        std::array<Offset, kNoteOffsets> offsets;
        MapDef<float, 5> lfo_freq_map;
        LightSensorTarget light_sensor_target;
    };

    static constexpr auto kResonanceMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {0.1f, 0.5f, 0.7f, 0.8f, 0.95f}};

    /**
     * @brief LFO range: 15 minutes to 10 Hz
     */
    static constexpr MapDef<float, 5> kDefaultLfoFreqMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {s2hz(15.0f * 60.0f),
         s2hz(10.0f),
         s2hz(2.0f),
         s2hz(1.0f), 10.0f}};

    /**
     * @brief Default spread: the first oscillator stays centered, the other two lean slightly to the opposite sides.
     */
    static constexpr StereoMix kDefaultStereoMix = {
        OscMix{q15(0.35f), q15(0.35f)},
        OscMix{q15(0.2f), q15(0.45f)},
        OscMix{q15(0.45f), q15(0.2f)}};

    /**
     * @brief Dark, resonant underwater bed.
     */
    static constexpr Specs kSpecsUnderWater = {
        .flavor = Flavor::LFO_FILTER_SWEEP,
        .filter_map = MapDef<float, 3>{
            {pot(0.0f), pot(0.5f), pot(1.0f)},
            {100.0f, 200.0f, 800.0f}},
        .filter_multiplier = 3000.0f,
        .default_filter = pot(0.01f),
        .default_resonance = pot(0.99f),
        .volume_boost = false,
        .waveforms = {Oscillator::Waveform::SQUARE, Oscillator::Waveform::RAMP, Oscillator::Waveform::SAW},
        .stereo_mix = kDefaultStereoMix,
        .offsets = {Offset{0, 3, 12}, Offset{0, 4, 12}, Offset{0, 5, 24}, Offset{0, 7, 13}, Offset{0, 3, 24}, Offset{0, 3, 37}, Offset{0, -12, 22}},
        .lfo_freq_map = kDefaultLfoFreqMap,
        .light_sensor_target = LightSensorTarget::LFO_DEPTH};

    /**
     * @brief Bright clustered harmonics with LFO-driving oscillator offsets.
     */
    static constexpr Specs kSpecsDigitalHive = {
        .flavor = Flavor::LFO_OFFSET_SWEEP,
        .filter_map = MapDef<float, 3>{
            {pot(0.0f), pot(0.5f), pot(1.0f)},
            {300.0f, 1000.0f, 8000.0f}},
        .filter_multiplier = 3000.0f,
        .default_filter = pot(0.8f),
        .default_resonance = pot(0.5f),
        .volume_boost = false,
        .waveforms = {Oscillator::Waveform::SINE, Oscillator::Waveform::RAMP, Oscillator::Waveform::SAW},
        .stereo_mix = kDefaultStereoMix,
        .offsets = {Offset{0, 15, 22}, Offset{0, 16, 24}, Offset{0, -10, 32}, Offset{0, -4, 25}, Offset{0, 4, 48}, Offset{0, 3, 37}, Offset{0, -20, 22}},
        .lfo_freq_map = kDefaultLfoFreqMap,
        .light_sensor_target = LightSensorTarget::OSC_OCTAVES};

    /**
     * @brief Noise-heavy thunder texture with broad filter motion.
     */
    static constexpr Specs kSpecsRainThunder = {
        .flavor = Flavor::LFO_FILTER_SWEEP,
        .filter_map = MapDef<float, 3>{
            {pot(0.0f), pot(0.5f), pot(1.0f)},
            {100.0f, 500.0f, 3000.0f}},
        .filter_multiplier = 6000.0f,
        .default_filter = pot(0.25f),
        .default_resonance = pot(0.2f),
        .volume_boost = true,
        .waveforms = {Oscillator::Waveform::NOISE, Oscillator::Waveform::NOISE, Oscillator::Waveform::RAMP},
        .stereo_mix = kDefaultStereoMix,
        .offsets = {Offset{0, 0, 0}, Offset{0, 0, 0}, Offset{0, 0, 0}, Offset{0, 0, 0}, Offset{0, 0, 0}, Offset{0, 0, 0}, Offset{0, 0, 0}},
        .lfo_freq_map = kDefaultLfoFreqMap,
        .light_sensor_target = LightSensorTarget::LFO_DEPTH};

    /**
     * @brief Digital bird singing.
     */
    static constexpr Specs kSpecsBirdSong = {
        .flavor = Flavor::LFO_OFFSET_SWEEP,
        .filter_map = MapDef<float, 3>{
            {pot(0.0f), pot(0.5f), pot(1.0f)},
            {800.0f, 3000.0f, 10000.0f}},
        .filter_multiplier = 3000.0f,
        .default_filter = pot(0.7f),
        .default_resonance = pot(0.25f),
        .volume_boost = true,
        .waveforms = {Oscillator::Waveform::SINE, Oscillator::Waveform::TRI, Oscillator::Waveform::SINE},
        .stereo_mix = kDefaultStereoMix,
        .offsets = {Offset{0, 5, 12}, Offset{0, 5, 17}, Offset{0, 7, 17}, Offset{0, 7, 29}, Offset{0, 12, 19}, Offset{0, 7, 24}, Offset{0, 19, 31}},
        .lfo_freq_map = MapDef<float, 5>{{pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)}, {0.3f, 1.0f, 4.0f, 10.0f, 20.0f}},
        .light_sensor_target = LightSensorTarget::LFO_DEPTH};

    PresetDrone(const Config &config, const Specs &specs) : Preset(config), specs_(specs) {}
    PresetDrone(const Config &config) = delete;

    // We cannot update all parameters in one go, because some of them are expensive to compute
    static constexpr uint32_t kParamsUpdateCounterMax = 3;

    static constexpr int32_t kMinLightSensorOctaveOffset = -2;
    static constexpr int32_t kMaxLightSensorOctaveOffset = 3;

    /**
     *  @brief Runs only once on the startup. Sets default positions for local preset pots.
     */
    void Init() override
    {
        preset_pots_[Pot::ALPHA] = pot(0.0f);
        preset_pots_[Pot::BETA] = specs_.default_filter;
        preset_pots_[Pot::GAMMA] = specs_.default_resonance;
    }

    /**
     * @brief Updates parameters.
     * @param params Current parameters (UI, MIDI, sequencer...)
     */
    void UpdateParams(const Params &params) override
    {
        auto &dsp = config_.dsp;

        // Combine light sensor values into a single modulation value
        const q15_t light_sensors_avg = LightSensorsAverage(params);

        // LFO waveform blend
        lfo_waveform_blend_.Compute(pot_to_q15(params.global_pots[Pot::BETA]), kLfoWaveformBlendOrder);

        // LFO speed
        dsp.lfo.SetFrequency(curve_map(params.global_pots[Pot::ALPHA], specs_.lfo_freq_map, MapClamp::TRUE, MapSafe::FALSE));

        // LFO depth
        q15_t new_lfo_depth = pot_to_q15(params.global_pots[Pot::GAMMA]);

        // Apply light sensor modulation to LFO depth if configured
        if (specs_.light_sensor_target == LightSensorTarget::LFO_DEPTH)
        {
            new_lfo_depth = q15_add(new_lfo_depth, light_sensors_avg);
            new_lfo_depth = constrain(new_lfo_depth, Q15_ZERO, Q15_MAX);
        }
        lfo_depth_ = new_lfo_depth;

        // Oscillators
        int32_t base_note = params.base_note + config_.octave_offset * 12;

        // Retrieve base note from MIDI (if available)
        if (params.incoming_midi_note.note_on)
        {
            midi_base_note_ = params.incoming_midi_note.note;
        }
        if (params.play_mode == PlayMode::MIDI && midi_base_note_ != INT32_MIN)
        {
            base_note = midi_base_note_;
        }

        if (specs_.light_sensor_target == LightSensorTarget::OSC_OCTAVES)
        {
            const int32_t octave_offset = map(
                light_sensors_avg,
                Q15_MIN,
                Q15_MAX,
                kMinLightSensorOctaveOffset,
                kMaxLightSensorOctaveOffset,
                MapClamp::TRUE,
                MapSafe::TRUE);
            base_note += octave_offset * 12;
        }
        size_t frequency_offset_index = pot_to_size(preset_pots_[Pot::ALPHA], specs_.offsets.size() - 1);
        size_t osc_index = params_update_counter_ % dsp.oscillators.size();
        Osc osc = static_cast<Osc>(osc_index);
        float note_offset = specs_.offsets[frequency_offset_index][osc];
        if (specs_.flavor == Flavor::LFO_OFFSET_SWEEP)
        {
            const float lfo_ratio = q15_to_float(q15_mult(lfo_value_, lfo_depth_));
            note_offset = note_offset * lfo_ratio;
        }
        float pitch_hz = midi_note_to_freq(static_cast<float>(base_note) + note_offset + params.pitch_bend);
        dsp.oscillators[osc].SetFrequency(pitch_hz);

        // Filter
        if (params_update_counter_ % 2 == 0)
        {
            float filter_hz = curve_map(preset_pots_[Pot::BETA], specs_.filter_map, MapClamp::TRUE, MapSafe::FALSE);
            if (specs_.flavor == Flavor::LFO_FILTER_SWEEP)
            {
                filter_hz += q15_to_float(q15_mult(lfo_value_, lfo_depth_)) * specs_.filter_multiplier;
            }
            for (auto &filter : dsp.filters)
            {
                filter.SetFrequency(filter_hz);
            }
        }
        if (params_update_counter_ % 2 == 1)
        {
            for (auto &filter : dsp.filters)
            {
                filter.SetResonance(curve_map(preset_pots_[Pot::GAMMA], kResonanceMap, MapClamp::TRUE, MapSafe::FALSE));
            }
        }

        params_update_counter_ = (params_update_counter_ + 1) % kParamsUpdateCounterMax;
    }

    /**
     * @brief Generates the audio.
     * @param output_buffer Destination block of stereo frames; both channels get the same signal for now.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer) override
    {
        auto &dsp = config_.dsp;

        const auto &lfo_out = dsp.lfo.Process();
        const q15_t lfo_blended = lfo_waveform_blend_.ProcessQ31(lfo_out);
        lfo_value_ = q15_unipolar(lfo_blended);

        for (size_t i = 0u; i < output_buffer.size(); ++i)
        {
            q15_t sample_left = 0;
            q15_t sample_right = 0;
            for (auto osc : EnumRange<Osc>())
            {
                const q15_t osc_sample = dsp.oscillators[osc].Process();
                const OscMix &mix = specs_.stereo_mix[osc];
                sample_left = q15_add(sample_left, q15_mult(osc_sample, mix.left));
                sample_right = q15_add(sample_right, q15_mult(osc_sample, mix.right));
            }

            sample_left = dsp.filters[Filter::FIRST].Process(sample_left);
            sample_right = dsp.filters[Filter::SECOND].Process(sample_right);

            // boost (multiplying by the reciprocal, the M0+ has no hardware divider)
            if (specs_.volume_boost)
            {
                sample_left = q15_mult_q4_12(sample_left, q4_12(1.0f / 0.75f));
                sample_right = q15_mult_q4_12(sample_right, q4_12(1.0f / 0.75f));
            }

            output_buffer[i] = {sample_left, sample_right};
        }
    }

    /**
     * @brief Drones deliberately opt out of aftertouch, despite sounding continuously.
     * @details A drone is not gated by the note at all - a MIDI note only retunes it - so pressure
     *          would ride its level indefinitely, including long after the note was released.
     *          Riding a drone's level is what the volume knob is for. Note-on velocity still applies.
     */
    bool AllowAftertouch() const override
    {
        return false;
    }

    /**
     * @brief Configures the DSP blocks when entering the preset.
     */
    void Enter() override
    {
        auto &dsp = config_.dsp;

        for (auto osc : EnumRange<Osc>())
        {
            dsp.oscillators[osc].SetWaveform(specs_.waveforms[osc]);
        }
        for (auto &filter : dsp.filters)
        {
            filter.SetType(Svf::Type::LOWPASS);
            filter.SetFrequency(8000.0f);
        }
    }

    /**
     * @brief Width of the ramp used to fade the LED in as lfo_value_ approaches the threshold.
     */
    static constexpr q15_t kLedRampWidth = q15(0.2f);

    /**
     * @brief Returns the LED state based on the LFO value and depth.
     */
    bool GetLedState() const override
    {
        return lfo_value_ > q15_sub(q15_inv(lfo_depth_), kLedRampWidth);
    }

    /**
     * @brief Fades the LED in as lfo_value_ crosses the same threshold used by GetLedState(),
     * ramping smoothly over kLedRampWidth instead of switching abruptly.
     */
    uint8_t GetLedBrightness() const override
    {
        const q15_t threshold = q15_sub(q15_inv(lfo_depth_), kLedRampWidth);
        const q15_t excess = constrain(q15_sub(lfo_value_, threshold), Q15_ZERO, kLedRampWidth);
        return static_cast<uint8_t>(q15_div(excess, kLedRampWidth) >> 7);
    }

    /**
     * @brief This preset randomizes all pots (not just MELODY and RHYTHM as the other presets do).
     */
    RandomizeType GetRandomizeType() const override
    {
        return RandomizeType::ALL_POTS;
    }

private:
    const Specs &specs_;
    EnumBlender<MultiOsc::Waveform> lfo_waveform_blend_{};
    q15_t lfo_depth_ = 0;
    q15_t lfo_value_ = 0;
    size_t params_update_counter_ = 0;
    int32_t midi_base_note_ = INT32_MIN;

    q15_t LightSensorsAverage(const Params &params) const
    {
        q15_t result = 0;
        static constexpr q15_t kMult = q15(1.0f / static_cast<float>(AnalogInputs::LightSensor::COUNT));
        for (auto sensor : EnumRange<LightSensor>())
        {
            result = q15_add(result, q15_mult(params.light_sensors[sensor], kMult));
        }
        return result;
    }
};

}