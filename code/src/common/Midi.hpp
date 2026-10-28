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
#include <array>
#include <cstdint>
#include "common/Color.hpp"

namespace badger
{

/**
 * @brief MIDI message types and constants.
 * @author Vaclav Mach
 * @date 2026-06-05
 */
class Midi
{
public:
    /**
     * @brief MIDI status/message types.
     * @details Channel Voice values use the high nibble as status and the low nibble for channel.
     */
    enum class Type : uint8_t
    {
        // Channel Voice Messages (high nibble, channel in low nibble)
        NOTE_OFF = 0x80,
        NOTE_ON = 0x90,
        POLYPHONIC_AFTERTOUCH = 0xA0,
        CONTROL_CHANGE = 0xB0,
        PROGRAM_CHANGE = 0xC0,
        CHANNEL_AFTERTOUCH = 0xD0,
        PITCH_BEND = 0xE0,

        // System Common Messages
        SYSTEM_EXCLUSIVE = 0xF0,
        MTC_QUARTER_FRAME = 0xF1,
        SONG_POSITION_POINTER = 0xF2,
        SONG_SELECT = 0xF3,
        TUNE_REQUEST = 0xF6,
        END_OF_EXCLUSIVE = 0xF7,

        // System Realtime Messages
        CLOCK = 0xF8,
        START = 0xFA,
        CONTINUE = 0xFB,
        STOP = 0xFC,
        ACTIVE_SENSING = 0xFE,
        SYSTEM_RESET = 0xFF
    };

    /**
     * @brief Supported MIDI Control Change (CC) identifiers used by the device.
     */
    enum class ControlChange : uint8_t
    {
        PRESET = 1,
        SCALE = 14,
        ROOT_NOTE = 15,
        OCTAVE = 16,
        GLOBAL_ALPHA = 17,
        GLOBAL_BETA = 18,
        GLOBAL_GAMMA = 19,
        PRESET_ALPHA = 20,
        PRESET_BETA = 21,
        PRESET_GAMMA = 22,
        ALL_SOUND_OFF = 120,
        ALL_NOTES_OFF = 123
    };

    /**
     * @brief MIDI message structure for encapsulating a MIDI event.
     */
    struct Message
    {
        Type type;
        uint8_t channel; ///> For channel messages (0-15)
        uint8_t data1;   ///> Note number, control number, etc.
        uint8_t data2;   ///> Velocity, control value, etc.
    };

    /**
     * @brief Base MIDI note number for C4 (middle C).
     * @details This is used as a reference point for note calculations and mappings.
     */
    static constexpr uint8_t BASE_NOTE = 60;

    /**
     * @brief Special value for omni mode (receive on all channels).
     * @details When set to this value, the device will respond to MIDI messages on all channels, regardless of the channel specified in the message.
     * @note For MIDI out a channel must be specified (usually falls back to channel 0 if omni is used).
     */
    static constexpr uint8_t OMNI_CHANNEL = 255;

    /**
     * @brief Color coding for MIDI channels in the UI. The array index corresponds to the MIDI channel (0-15), and the value is the associated color.
     */
    static constexpr std::array<Color, 16> kChannelColors = {
        Color::BLUE,
        Color::CYAN,
        Color::GREEN,
        Color::MAGENTA,
        Color::RED,
        Color::YELLOW,
        Color::BLUE,
        Color::CYAN,
        Color::GREEN,
        Color::MAGENTA,
        Color::RED,
        Color::YELLOW,
        Color::BLUE,
        Color::CYAN,
        Color::GREEN,
        Color::MAGENTA};

    /**
     * @brief Color for omni mode (receive on all channels).
     */
    static constexpr Color kOmniChannelColor = Color::WHITE;

    /**
     * @brief Pitch bend range in semitones.
     * @details This value defines the range of pitch bend that can be applied to notes.
     */
    static constexpr float kPitchBendRangeSemitones = 7.0f;
};

}