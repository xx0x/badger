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

#include "presets/Preset.hpp"
#include "presets/PresetBassGuitar.hpp"
#include "presets/PresetChiptune.hpp"
#include "presets/PresetDrone.hpp"
#include "presets/PresetDrums.hpp"
#include "presets/PresetFm.hpp"
#include "presets/PresetOrgan.hpp"
#include "presets/PresetPercussions.hpp"
#include "presets/PresetStereoTest.hpp"
#include "presets/PresetSynth.hpp"
#include "presets/PresetWitchSaw.hpp"

namespace badger
{

/**
 * @brief Preset bank manager for all available sound engines.
 * @note The presets are all stack-allocated - not ideal for saving RAM but we are sure it won't crash due to insufficient memory.
 * @details
 * Owns all preset instances, shared DSP resources, and shared runtime params.
 * Handles preset switching and local preset-pot persistence conversion.
 *
 * Preset headers in this folder follow two patterns:
 * - Standalone preset class per header (for example PresetSynth, PresetBassGuitar...).
 * - Spec-driven preset class where one implementation exposes multiple variants via static specs (for example PresetFm, PresetDrone...).
 * @author Vaclav Mach
 * @date 2026-06-11
 */
class Presets
{
public:
    /**
     * @brief Total number of presets.
     */
    static constexpr size_t kNumPresets = 18;

    /**
     * @brief Compact storage format for local preset pots.
     * Uses 8-bit compact form to save space, while runtime uses 12-bit ADC-style values.
     * There is some loss of precision when converting between 12-bit and 8-bit, but it's OK for pot values.
     */
    using PresetPotStorage = std::array<std::array<uint8_t, 3>, kNumPresets>;

    /**
     * @brief Initializes all presets and enters the currently selected preset.
     */
    void Init();

    /**
     * @brief Changes current preset index with wraparound.
     * @param offset Signed step; negative for previous, positive for next.
     */
    void Change(int32_t offset);

    /**
     * @brief Selects preset from raw MIDI CC value in range 0-127.
     * @details Maps the full CC range onto all available preset indexes.
     * @param cc_value Raw MIDI CC value.
     */
    void SelectFromCc(uint8_t cc_value);

    /**
     * @brief Returns the active preset.
     */
    Preset &GetCurrent()
    {
        return *presets_[current_preset_];
    }

    /**
     * @brief Returns the active preset index.
     */
    size_t GetCurrentPresetIndex() const
    {
        return current_preset_;
    }

    /**
     * @brief Returns shared runtime params consumed by presets in UpdateParams().
     */
    Preset::Params &GetParams()
    {
        return shared_params_;
    }

    /**
     * @brief Returns shared DSP objects used by all presets.
     */
    Preset::Dsp &GetDsp()
    {
        return shared_dsp_;
    }

    /**
     * @brief Serializes all preset local pots into compact 8-bit storage.
     * @return Compressed ALPHA/BETA/GAMMA pot values for each preset.
     */
    PresetPotStorage GetPresetPotStorage() const
    {
        PresetPotStorage preset_pot_storage;
        for (size_t i = 0; i < kNumPresets; ++i)
        {
            // Preset pots are compressed from 12 to 8 bits for storage
            const auto &pots = presets_[i]->GetPresetPots();
            preset_pot_storage[i] = {static_cast<uint8_t>(pots[Preset::Pot::ALPHA] >> 4),
                                     static_cast<uint8_t>(pots[Preset::Pot::BETA] >> 4),
                                     static_cast<uint8_t>(pots[Preset::Pot::GAMMA] >> 4)};
        }
        return preset_pot_storage;
    }

    /**
     * @brief Restores preset local pots from compact 8-bit storage.
     * @param preset_pots Stored ALPHA/BETA/GAMMA values for each preset.
     */
    void SetPresetPotsStorage(const PresetPotStorage &preset_pots)
    {
        for (size_t i = 0; i < kNumPresets; ++i)
        {
            // Expand preset pots from storage size 8 to regular 12 bits
            Preset::PresetPots pots = {static_cast<uint16_t>(preset_pots[i][0] << 4),
                                       static_cast<uint16_t>(preset_pots[i][1] << 4),
                                       static_cast<uint16_t>(preset_pots[i][2] << 4)};
            presets_[i]->SetPresetPots(pots);
        }
    }

private:
    /**
     * @brief Shared DSP backend reused by every preset instance.
     */
    Preset::Dsp shared_dsp_;

    /**
     * @brief Shared runtime parameter block updated by Application.
     */
    Preset::Params shared_params_;

    /**
     *  @brief Index of currently active preset in presets_.
     */
    size_t current_preset_ = 0;

    /**
     *  @brief Group color used by melody presets.
     */
    static constexpr Color kMelodyGroupColor = Color::WHITE;

    /**
     *  @brief Group color used by drum/percussion presets.
     */
    static constexpr Color kDrumsGroupColor = Color::BLUE;

    /**
     * @brief Group color used by drone presets.
     */
    static constexpr Color kDroneGroupColor = Color::GREEN;

    // Temporary bench preset for the stereo audio path, kept first so it is easy to reach
    PresetStereoTest preset_stereo_test_{Preset::Config{.color = Color::BLUE,
                                                        .group_color = Color::BLUE,
                                                        .volume_compensation = q15(1.0f),
                                                        .dsp = shared_dsp_}};

    // Melody presets (White)
    PresetSynth preset_synth_{Preset::Config{.color = Color::YELLOW,
                                             .group_color = kMelodyGroupColor,
                                             .volume_compensation = q15(1.0f),
                                             .dsp = shared_dsp_}};

    PresetWitchSaw preset_witch_saw_{{.color = Color::MAGENTA,
                                      .group_color = kMelodyGroupColor,
                                      .octave_offset = 1,
                                      .volume_compensation = q15(0.9f),
                                      .dsp = shared_dsp_}};

    PresetOrgan preset_organ_{Preset::Config{.color = Color::RED,
                                             .group_color = kMelodyGroupColor,
                                             .volume_compensation = q15(0.9f),
                                             .dsp = shared_dsp_}};

    PresetFm preset_electric_piano_{{.color = Color::GREEN,
                                     .group_color = kMelodyGroupColor,
                                     .octave_offset = 1,
                                     .volume_compensation = q15(0.5f),
                                     .dsp = shared_dsp_},
                                    PresetFm::kSpecsPiano};

    PresetFm preset_bell_{{.color = Color::BLUE,
                           .group_color = kMelodyGroupColor,
                           .octave_offset = 2,
                           .volume_compensation = q15(0.5f),
                           .dsp = shared_dsp_},
                          PresetFm::kSpecsBell};

    PresetFm preset_flute_{{.color = Color::CYAN,
                            .group_color = kMelodyGroupColor,
                            .octave_offset = 2,
                            .volume_compensation = q15(0.5f),
                            .dsp = shared_dsp_},
                           PresetFm::kSpecsDigiFlute};

    PresetChiptune preset_chiptune_{Preset::Config{.color = Color::MAGENTA,
                                                   .group_color = kMelodyGroupColor,
                                                   .octave_offset = 1,
                                                   .volume_compensation = q15(0.5f),
                                                   .dsp = shared_dsp_}};

    PresetBassGuitar preset_bass_guitar_{{.color = Color::RED,
                                          .group_color = kMelodyGroupColor,
                                          .octave_offset = -2,
                                          .volume_compensation = q15(0.75f),
                                          .dsp = shared_dsp_}};

    // Drum presets (Blue except for the one which is Cyan to differ from group color)
    PresetDrums preset_classic_drums_{{.color = Color::YELLOW,
                                       .group_color = kDrumsGroupColor,
                                       .octave_offset = -1,
                                       .volume_compensation = q15(1.0f),
                                       .dsp = shared_dsp_},
                                      PresetDrums::kSpecsClassic};

    PresetDrums preset_8bit_drums_{{.color = Color::MAGENTA,
                                    .group_color = kDrumsGroupColor,
                                    .octave_offset = -1,
                                    .volume_compensation = q15(0.8f),
                                    .dsp = shared_dsp_},
                                   PresetDrums::kSpecs8Bit};

    PresetPercussions preset_tambourine_{{.color = Color::GREEN,
                                          .group_color = kDrumsGroupColor,
                                          .octave_offset = 0,
                                          .volume_compensation = q15(0.8f),
                                          .dsp = shared_dsp_},
                                         PresetPercussions::kSpecsTambourine};

    PresetPercussions preset_woodblock_{{.color = Color::BLUE,
                                         .group_color = Color::CYAN,
                                         .octave_offset = 1,
                                         .volume_compensation = q15(0.8f),
                                         .dsp = shared_dsp_},
                                        PresetPercussions::kSpecsWoodblock};

    PresetPercussions preset_clap_{{.color = Color::CYAN,
                                    .group_color = kDrumsGroupColor,
                                    .octave_offset = 4,
                                    .volume_compensation = q15(0.8f),
                                    .dsp = shared_dsp_},
                                   PresetPercussions::kSpecsClap};

    PresetDrums preset_laser_drum_{{.color = Color::RED,
                                    .group_color = kDrumsGroupColor,
                                    .octave_offset = 3,
                                    .volume_compensation = q15(.8f),
                                    .dsp = shared_dsp_},
                                   PresetDrums::kSpecsLaser};

    // Drone presets (Green except for the one which is Cyan to differ from group color)
    PresetDrone preset_drone_rain_thunder_{{.color = Color::YELLOW,
                                            .group_color = kDroneGroupColor,
                                            .octave_offset = -1,
                                            .volume_compensation = q15(0.9f),
                                            .dsp = shared_dsp_},
                                           PresetDrone::kSpecsRainThunder};

    PresetDrone preset_drone_digital_hive_{{.color = Color::MAGENTA,
                                            .group_color = kDroneGroupColor,
                                            .octave_offset = 1,
                                            .volume_compensation = q15(0.66f),
                                            .dsp = shared_dsp_},
                                           PresetDrone::kSpecsDigitalHive};

    PresetDrone preset_drone_under_water_{{.color = Color::BLUE,
                                           .group_color = kDroneGroupColor,
                                           .octave_offset = -2,
                                           .volume_compensation = q15(.8f),
                                           .dsp = shared_dsp_},
                                          PresetDrone::kSpecsUnderWater};

    PresetDrone preset_drone_bird_song_{{.color = Color::CYAN,
                                         .group_color = kDroneGroupColor,
                                         .octave_offset = 2,
                                         .volume_compensation = q15(0.9f),
                                         .dsp = shared_dsp_},
                                        PresetDrone::kSpecsBirdSong};

    /**
     * @brief Array of preset pointers for easy access.
     */
    std::array<Preset *, kNumPresets> presets_ = {

        // Stereo bench test
        // &preset_stereo_test_,

        // Melodies
        &preset_synth_,
        &preset_witch_saw_,
        &preset_organ_,
        &preset_electric_piano_,
        &preset_bell_,
        &preset_flute_,
        &preset_chiptune_,
        &preset_bass_guitar_,

        // Drums
        &preset_classic_drums_,
        &preset_8bit_drums_,
        &preset_tambourine_,
        &preset_woodblock_,
        &preset_clap_,
        &preset_laser_drum_,

        // Drone
        &preset_drone_rain_thunder_,
        &preset_drone_digital_hive_,
        &preset_drone_under_water_,
        &preset_drone_bird_song_

    };
};

}