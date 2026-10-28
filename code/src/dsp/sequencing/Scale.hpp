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

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include "dsp/math/math_utils.hpp"

namespace badger
{

/**
 * @brief Pitch-class set over 12 semitones with random note generation support.
 * @author Vaclav Mach
 * @date 2026-06-10
 * @example Scale{0, 2, 4, 5, 7, 9, 11}; // Major Diatonic
 */
class Scale
{
public:
    /**
     * @brief Semitone note value type (pitch class when in [0..11]).
     */
    using Note = int8_t;

    /**
     * @brief Maximum number of pitch classes in one octave.
     */
    static constexpr uint8_t kMaxNotes = 12;

    /**
     * @brief Construct an empty scale.
     */
    constexpr Scale() = default;

    /**
     * @brief Construct scale from semitone offsets in one octave.
     * @param notes Pitch classes in [0..11].
     */
    constexpr Scale(std::initializer_list<Note> notes)
    {
        for (auto n : notes)
        {
            if (n >= 0 && n < kMaxNotes)
            {
                mask_ |= (uint16_t(1u) << n);
            }
        }
    }

    /**
     * @brief Number of active pitch classes in the scale.
     */
    constexpr uint8_t size() const noexcept
    {
        return popcount12(mask_);
    }

    /**
     * @brief Check whether a pitch class is part of the scale.
     * @param n Pitch class to test.
     */
    constexpr bool contains(Note n) const noexcept
    {
        return (n >= 0 && n < kMaxNotes) && (mask_ & (uint16_t(1u) << n));
    }

    /**
     * @brief Pick a random note from the scale with octave transposition.
     * @tparam Rng Callable returning an unsigned integer random value.
     * @param rng Random generator callable.
     * @param lower_octave_span Lower inclusive octave offset.
     * @param upper_octave_span Upper inclusive octave offset.
     * @return Random note where pitch class is in scale and octave offset is in requested range.
     */
    template <typename Rng>
    Note RandomNote(Rng &&rng, int8_t lower_octave_span = 0, int8_t upper_octave_span = 0) const
    {
        uint8_t count = size();
        assert(count != 0);
        if (count == 0)
        {
            return 0;
        }

        uint8_t pick = uint8_t(rng() % count);
        uint8_t pc = NthSetBit(pick);

        int8_t lower = lower_octave_span;
        int8_t upper = upper_octave_span;
        if (lower > upper)
        {
            const int8_t tmp = lower;
            lower = upper;
            upper = tmp;
        }

        const uint32_t range = static_cast<uint32_t>(static_cast<int32_t>(upper) - static_cast<int32_t>(lower) + 1);
        const int8_t octave = static_cast<int8_t>(static_cast<int32_t>(lower) + static_cast<int32_t>(rng() % range));

        return int8_t(pc + octave * 12);
    }

    /**
     * @brief Returns a note from the scale closes to the given note
     * @param note Target note
     * @return Closest note
     */
    constexpr int8_t ClosestTo(int8_t note) const
    {
        static constexpr int32_t kSemitonesPerOctave = 12;

        const int32_t note_value = static_cast<int32_t>(note);
        const int32_t pitch_class = positive_modulo(note_value, kSemitonesPerOctave);

        if (contains(static_cast<Scale::Note>(pitch_class)))
        {
            return note;
        }

        const int32_t base_octave = (note_value - pitch_class) / kSemitonesPerOctave;

        int32_t best_note = note_value;
        int32_t best_distance = 0x7FFFFFFF;

        for (int32_t scale_note = 0; scale_note < kSemitonesPerOctave; ++scale_note)
        {
            if (!contains(static_cast<Scale::Note>(scale_note)))
            {
                continue;
            }

            for (int32_t octave_offset = -1; octave_offset <= 1; ++octave_offset)
            {
                const int32_t candidate = (base_octave + octave_offset) * kSemitonesPerOctave + scale_note;
                const int32_t distance = (candidate >= note_value) ? (candidate - note_value) : (note_value - candidate);

                if ((distance < best_distance) || (distance == best_distance && candidate < best_note))
                {
                    best_distance = distance;
                    best_note = candidate;
                }
            }
        }

        return static_cast<int8_t>(best_note);
    }

private:
    /**
     * @brief Popcount implementation limited to 12-bit masks.
     */
    static constexpr uint8_t popcount12(uint16_t x) noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_popcount(x);
#else
        uint8_t c = 0;
        for (uint8_t i = 0; i < kMaxNotes; ++i)
            c += (x >> i) & 1u;
        return c;
#endif
    }

    /**
     * @brief Return the index-th active pitch class from the internal bitmask.
     */
    constexpr uint8_t NthSetBit(uint8_t index) const noexcept
    {
        uint16_t m = mask_;

        for (uint8_t i = 0; i < kMaxNotes; ++i)
        {
            if (m & 1u)
            {
                if (index == 0)
                    return i;
                --index;
            }
            m >>= 1;
        }

        return 0;
    }

    /**
     * @brief Bitmask of active pitch classes in one octave.
     */
    uint16_t mask_ = 0;
};

}