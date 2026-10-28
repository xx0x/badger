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

#include <cstddef>
#include <cstdint>

#include "app/Presets.hpp"
#include "common/Midi.hpp"
#include "dsp/math/qmath.hpp"
#include "dsp/sequencing/TrackSequencer.hpp"

namespace badger
{

/**
 * @brief Persisted application state stored on the on-board EEPROM.
 * @details
 * This payload is wrapped by SaveFileHandler records and validated via
 * magic markers and a record checksum when loading.
 * @author Vaclav Mach
 * @date 2026-06-21
 */
struct SaveFile
{
    /** @brief Save format start marker. */
    static constexpr uint32_t kMagicStart = 0x0BAD6343;

    /**
     * @brief Save format version.
     * @note Bump this whenever the layout below changes. A record with a different version is
     *       discarded and the device starts from the defaults, there are no migrations. Without it,
     *       a layout change that happens to keep the same size would be read back as a valid
     *       record full of misinterpreted fields.
     * @details The field sits in what used to be padding after scale_index, which was zero in every
     *          record written before the field existed. Those records therefore read as version 0
     *          and stay valid.
     */
    static constexpr uint8_t kVersion = 0;

    /** @brief Save format end marker. */
    static constexpr uint32_t kMagicEnd = 0xFBAD6351;

    /**
     * @brief Master volume a factory-reset or blank device starts at.
     * @note Application::volume_ starts from this too, so there is only one default to change.
     */
    static constexpr q15_t kDefaultVolume = q15(0.125f);

    uint32_t magic_start = kMagicStart;
    q15_t volume = kDefaultVolume;
    uint8_t root_note = 0;
    uint8_t scale_index = TrackSequencer::kChromaticScaleIndex;
    uint8_t version = kVersion;
    uint32_t internal_clock_pulse_period = 0;
    size_t midi_clock_divider_index = SIZE_MAX;
    size_t external_clock_divider_multiplier_index = SIZE_MAX;
    uint8_t midi_channel = Midi::OMNI_CHANNEL;
    Presets::PresetPotStorage preset_pots_storage;
    uint32_t magic_end = kMagicEnd;

    /** @brief Checks save integrity markers and that the record has the current layout. */
    constexpr bool IsValid() const
    {
        return magic_start == kMagicStart &&
               version == kVersion &&
               magic_end == kMagicEnd;
    }

    /** @brief Equality comparison for change detection. */
    bool operator==(const SaveFile &other) const = default;
};

/**
 * @brief Size of SaveFile in bytes, used for EEPROM storage allocation.
 */
static constexpr size_t kSaveFileSize = sizeof(SaveFile);

}