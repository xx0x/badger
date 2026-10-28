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
#include <tuple>
#include "Scale.hpp"
#include "common/Color.hpp"
#include "dsp/math/math_utils.hpp"
#include "utils/EnumTools.hpp"

namespace badger
{

/**
 * @brief Two-track step sequencer for rhythm gates and melody offsets.
 * @details
 * The sequencer stores predefined rhythm and melody pattern sets, supports
 * chord/scale-aware quantization, and exposes trigger state for each track.
 * @author Vaclav Mach
 * @date 2026-06-10
 */
class TrackSequencer
{
public:
    /**
     * @brief Tracks produced by the sequencer.
     */
    enum class Track
    {
        PRIMARY,
        SECONDARY,
        COUNT
    };

    /**
     * @brief Base note (C3) in MIDI note numbers.
     */
    static constexpr int32_t kBaseNote = 48;

    /**
     * @brief Number of available root notes for melody generation, corresponding to the 12 semitones in an octave.
     * @warning Don't change this!
     */
    static constexpr size_t kRootNotes = 12;

    /**
     * @brief Maximum number of sequencer steps.
     */
    static constexpr size_t kSequencerMaxLength = 16;

    /**
     * @brief Number of built-in rhythm track sets.
     */
    static constexpr size_t kRhythmTrackSetsCount = 21;

    /**
     * @brief Number of built-in melody track sets.
     */
    static constexpr size_t kMelodyTrackSetsCount = 21;

    /**
     * @brief Minimum octave offset for melody generation (inclusive).
     */
    static constexpr int32_t kMinOctaveOffset = -3;

    /**
     * @brief Maximum octave offset for melody generation (inclusive).
     */
    static constexpr int32_t kMaxOctaveOffset = 3;

    /**
     * @brief Octaves are color-coded in the UI based on the following mapping (offset, color).
     */
    static constexpr std::array<std::tuple<int32_t, Color>, 7> kOctaveColors = {
        std::make_tuple(-3, Color::BLUE),
        std::make_tuple(-2, Color::CYAN),
        std::make_tuple(-1, Color::GREEN),
        std::make_tuple(0, Color::WHITE),
        std::make_tuple(1, Color::MAGENTA),
        std::make_tuple(2, Color::RED),
        std::make_tuple(3, Color::YELLOW)};

    /**
     * @brief Root notes color coding.
     */
    static constexpr std::array<Color, kRootNotes> kRootColors = {
        Color::BLUE,    // C
        Color::CYAN,    // C#
        Color::GREEN,   // D
        Color::MAGENTA, // D#
        Color::RED,     // E
        Color::YELLOW,  // F
        Color::BLUE,    // F#
        Color::CYAN,    // G
        Color::GREEN,   // G#
        Color::MAGENTA, // A
        Color::RED,     // A#
        Color::YELLOW   // B
    };

    /**
     * @brief Bit-mask rhythm pattern pair, one pattern per track.
     */
    using RhythmTrackSet = EnumArray<Track, uint16_t>;

    /**
     * @brief Melody offsets for one track across all steps.
     */
    using MelodyTrack = std::array<int8_t, kSequencerMaxLength>;

    /**
     * @brief Per-track melody offsets for one full sequence length.
     */
    using MelodyTrackSet = EnumArray<Track, MelodyTrack>;

    /**
     * @brief Scale with associated color for UI indication.
     */
    struct ScaleWithColor
    {
        Color color;
        Scale scale;
    };

    /**
     * @brief Available quantization scales used by melody generation/mapping.
     */
    static constexpr std::array<ScaleWithColor, 7> kScales = {
        Color::BLUE, Scale{0, 3, 7},                               // Minor Chord
        Color::CYAN, Scale{0, 3, 5, 7, 10},                        // Minor Pentatonic
        Color::GREEN, Scale{0, 2, 3, 5, 7, 8, 10},                 // Minor Diatonic
        Color::WHITE, Scale{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, // Chromatic
        Color::MAGENTA, Scale{0, 2, 4, 5, 7, 9, 11},               // Major Diatonic
        Color::RED, Scale{0, 2, 4, 7, 9},                          // Major Pentatonic
        Color::YELLOW, Scale{0, 4, 7}                              // Major Chord
    };

    /**
     * @brief Index of chromatic scale in kScales.
     */
    static constexpr size_t kChromaticScaleIndex = 3;

    /**
     * @brief Scales used by melody randomizer when default chromatic is selected.
     * Doesn't need to match the kScales array in any means
     * Scales source: Bastl Instruments Kastle 2 series, MIT licensed
     */
    static constexpr std::array<Scale, 12> kRandomizerScales = {
        Scale{0, 3, 7},              // Minor Scale
        Scale{0, 3, 5, 7, 10},       // Minor Pentatonic
        Scale{0, 2, 3, 5, 7, 8, 10}, // Minor Diatonic
        Scale{0, 2, 4, 5, 7, 9, 11}, // Major Diatonic
        Scale{0, 2, 4, 7, 9},        // Major Pentatonic
        Scale{0, 4, 7},              // Major Scale
        Scale{0, 2, 3, 5, 7, 8, 11}, // Hungarian Minor
        Scale{0, 1, 3, 5, 7, 8, 10}, // Phrygian
        Scale{0, 2, 3, 5, 7, 8, 10}, // Aeolian
        Scale{0, 2, 4, 5, 7, 9, 11}, // Ionian
        Scale{0, 2, 4, 6, 7, 9, 11}, // Lydian
        Scale{0, 2, 4, 6, 8, 10}     // Wholetone
    };

    /**
     * @brief Built-in rhythm patterns.
     */
    static constexpr std::array<RhythmTrackSet, kRhythmTrackSetsCount> kRhythmTrackSets = {
        RhythmTrackSet{0b1000000000000000,
                       0b0000000010000000},
        RhythmTrackSet{0b1000000010000000,
                       0b0000000010000000},
        RhythmTrackSet{0b1000000010001000,
                       0b0000100000000000},
        RhythmTrackSet{0b1000100010001000,
                       0b0000000010000000},
        RhythmTrackSet{0b1010101010101010,
                       0b0010001000100010},
        RhythmTrackSet{0b1000001010000010,
                       0b0000100000001000},
        RhythmTrackSet{0b1001000010010000,
                       0b0000100000001000},
        RhythmTrackSet{0b1000101010001010,
                       0b0000100000001000},
        RhythmTrackSet{0b1000001001001000,
                       0b0001000001000010},
        RhythmTrackSet{0b1010000010000010,
                       0b0000100000101000},
        RhythmTrackSet{0b1000100010101000,
                       0b0000100000100010},
        RhythmTrackSet{0b1000101000101000,
                       0b0001001000010010},
        RhythmTrackSet{0b1001001010001010,
                       0b0000100000101000},
        RhythmTrackSet{0b1010001010100010,
                       0b0000100010001000},
        RhythmTrackSet{0b1010100010101010,
                       0b0000101000001010},
        RhythmTrackSet{0b1011001010110010,
                       0b0100100001001000},
        RhythmTrackSet{0b1010111010101110,
                       0b0010100010100010},
        RhythmTrackSet{0b1110101011101010,
                       0b0010101000101010},
        RhythmTrackSet{0b1110111010111010,
                       0b0101001001010010},
        RhythmTrackSet{0b1110111011101110,
                       0b0010101010101010},
        RhythmTrackSet{0b1111111111111111,
                       0b0101010101010101}};

    /**
     * @brief Built-in melody pattern sets.
     */
    static constexpr std::array<MelodyTrackSet, kMelodyTrackSetsCount> kMelodyTrackSets = {
        MelodyTrackSet{
            MelodyTrack{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            MelodyTrack{0, 4, 7, 12, 7, 4, 0, 4, 7, 12, 7, 4, 0, 4, 7, 12}},
        MelodyTrackSet{
            MelodyTrack{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0},
            MelodyTrack{0, 0, 7, 0, 0, 4, 0, 0, 12, 0, 0, 7, 0, 0, 4, 0}},
        MelodyTrackSet{
            MelodyTrack{0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 7},
            MelodyTrack{12, 0, 0, 7, 0, 0, 4, 0, 12, 0, 0, 7, 0, 0, 4, 0}},
        MelodyTrackSet{
            MelodyTrack{0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 7, 0, 0, 4, 0},
            MelodyTrack{7, 0, 4, 0, 7, 0, 12, 0, 4, 0, 7, 0, 12, 0, 4, 0}},
        MelodyTrackSet{
            MelodyTrack{0, 4, 7, 4, 0, 4, 7, 4, 0, 4, 7, 4, 0, 4, 7, 4},
            MelodyTrack{11, 9, 7, 5, 4, 2, 0, 2, 4, 5, 7, 9, 11, 9, 7, 5}},
        MelodyTrackSet{
            MelodyTrack{0, 2, 4, 5, 7, 5, 4, 2, 0, 2, 4, 5, 7, 5, 4, 2},
            MelodyTrack{7, 7, 4, 4, 2, 2, 0, 0, 7, 7, 9, 9, 11, 11, 12, 12}},
        MelodyTrackSet{
            MelodyTrack{0, 0, 3, 4, 7, 7, 4, 3, 0, 0, 3, 4, 7, 7, 4, 3},
            MelodyTrack{9, 7, 4, 2, 0, 2, 4, 5, 7, 9, 7, 5, 4, 2, 0, 4}},
        MelodyTrackSet{
            MelodyTrack{0, 0, -12, 0, 4, 0, 0, 12, 7, 0, 0, -12, 4, 0, 12, 0},
            MelodyTrack{12, 0, 7, 4, 0, -5, 0, 9, 0, 12, 4, 7, 0, -12, 11, 7}},
        MelodyTrackSet{
            MelodyTrack{0, 4, -12, 0, 7, 0, 4, 12, 0, 7, 0, 4, -12, 0, 7, 12},
            MelodyTrack{12, 0, 7, 2, 4, -12, 0, 9, 12, 4, 0, 7, 11, -12, 2, 4}},
        MelodyTrackSet{
            MelodyTrack{0, 4, 7, 12, 3, 0, -12, 4, 7, 12, 7, 4, 0, -12, 0, 3},
            MelodyTrack{4, 7, 11, 12, 9, 7, 5, 4, 7, 12, 11, 9, 7, 5, 4, 2}},
        MelodyTrackSet{
            MelodyTrack{0, 7, 4, 9, 2, 11, 5, 12, 7, 0, 9, 4, 11, 2, 12, 5},
            MelodyTrack{12, 11, 9, 7, 5, 4, 2, 0, 2, 4, 5, 7, 9, 11, 12, 14}},
        MelodyTrackSet{
            MelodyTrack{-12, 0, 2, 5, -12, 4, 7, 10, 0, 2, 5, 12, 0, 4, 7, 10},
            MelodyTrack{12, 9, 7, 5, 12, 7, 4, 2, 11, 9, 7, 5, 4, 2, 0, -1}},
        MelodyTrackSet{
            MelodyTrack{-12, 3, -10, 7, 0, 10, 2, 12, 5, -10, 7, 3, 10, 0, 2, -12},
            MelodyTrack{0, 2, 4, 7, 9, 11, 12, 11, 9, 7, 5, 4, 2, 0, -1, -3}},
        MelodyTrackSet{
            MelodyTrack{0, 7, 0, 4, 0, 7, 12, 7, 4, 0, 4, 7, 0, 12, 7, 4},
            MelodyTrack{0, 4, 7, 9, 12, 9, 7, 4, 2, 4, 7, 11, 12, 11, 9, 7}},
        MelodyTrackSet{
            MelodyTrack{0, 5, -1, 8, 3, 0, 5, -4, 8, 3, -1, 5, 0, 8, 3, -4},
            MelodyTrack{9, 7, 4, 2, 0, 4, 7, 11, 12, 9, 7, 4, 2, 0, -1, 4}},
        MelodyTrackSet{
            MelodyTrack{0, 7, -4, 3, 10, 5, -2, 12, 7, 0, 10, 3, -4, 5, 12, -2},
            MelodyTrack{12, 9, 7, 5, 4, 2, 0, -1, 7, 9, 11, 12, 9, 7, 5, 4}},
        MelodyTrackSet{
            MelodyTrack{-24, -17, -12, -5, 0, 4, 9, 12, 9, 4, 0, -5, -12, -17, -24, -12},
            MelodyTrack{12, 16, 21, 24, 21, 16, 12, 9, 7, 5, 4, 2, 0, -1, -3, 0}},
        MelodyTrackSet{
            MelodyTrack{-12, 9, -3, 14, 0, 16, -8, 7, 4, -5, 12, -12, 11, 2, 17, 5},
            MelodyTrack{0, 4, 7, 12, 16, 12, 7, 4, 0, -5, -12, -5, 0, 7, 12, 16}},
        MelodyTrackSet{
            MelodyTrack{0, 6, 11, 4, 9, 2, 7, 12, 16, 11, 6, 14, 9, 4, 12, 7},
            MelodyTrack{12, 7, 4, 9, 5, 11, 7, 0, 14, 12, 9, 7, 16, 11, 4, 2}},
        MelodyTrackSet{
            MelodyTrack{-12, 0, -6, 6, -3, 9, 0, 12, 3, -9, 6, -6, 9, -3, 12, 0},
            MelodyTrack{12, 9, 7, 4, 2, 0, -1, 0, -12, 0, 4, 7, 9, 11, 12, 7}},
        MelodyTrackSet{
            MelodyTrack{-12, 1, -9, 5, -7, 8, -4, 10, -2, 12, 0, 14, 3, 7, -5, 11},
            MelodyTrack{14, 11, 9, 7, 5, 4, 2, 0, 12, 9, 7, 4, 11, 7, 2, -12}}};

    /**
     * @brief Convert root note [0..11] to MIDI CC range [0..127].
     * @param root_note Root note index in semitones.
     */
    static constexpr uint8_t RootNoteToCc(size_t root_note)
    {
        return value_to_cc(root_note, 0, kRootNotes - 1);
    }

    /**
     * @brief Convert MIDI CC value [0..127] back to root note [0..11].
     * @param cc MIDI CC value.
     */
    static constexpr size_t CcToRootNote(uint8_t cc)
    {
        return static_cast<size_t>(cc_to_value(cc, 0, kRootNotes - 1));
    }

    /**
     * @brief Convert scale index to MIDI CC range [0..127].
     * @param scale_index Scale index.
     */
    static constexpr uint8_t ScaleToCc(size_t scale_index)
    {
        return value_to_cc(scale_index, 0, kScales.size() - 1);
    }

    /**
     * @brief Convert MIDI CC value [0..127] back to scale index.
     * @param cc MIDI CC value.
     */
    static constexpr size_t CcToScale(uint8_t cc)
    {
        return static_cast<size_t>(cc_to_value(cc, 0, kScales.size() - 1));
    }

    /**
     * @brief Convert MIDI CC value [0..127] to octave offset (-3..3).
     * @param cc MIDI CC value.
     */
    static constexpr int32_t CcToOctaveOffset(uint8_t cc)
    {
        return static_cast<int32_t>(cc_to_value(cc, kMinOctaveOffset, kMaxOctaveOffset));
    }

    /**
     * @brief Initialize runtime state.
     */
    void Init();

    /**
     * @brief Reset transport-dependent runtime state.
     */
    void Reset();

    /**
     * @brief Advance sequencer on clock tick and update trigger outputs.
     * @param clock_tick True to advance one step.
     */
    void Process(bool clock_tick);

    /**
     * @brief Select a built-in rhythm pattern set by index.
     */
    void SetRhythmTrackSet(size_t index);

    /**
     * @brief Select a built-in melody pattern set by index, then quantize to current scale.
     */
    void SetMelodyTrackSet(size_t index);

    /**
     * @brief Set active scale index and re-quantize current melody content.
     */
    void SetScaleIndex(size_t index);

    /**
     * @brief Get the currently selected scale index.
     */
    size_t GetScaleIndex() const
    {
        return scale_index_;
    }

    /**
     * @brief Randomize rhythm bit patterns for both tracks.
     */
    void RandomizeRhythmTrackSet();
    /**
     * @brief Randomize melody notes using the current scale.
     * @param lower_octave_span Number of octaves below the base note to include in the randomization.
     * @param upper_octave_span Number of octaves above the base note to include in the randomization.
     */
    void RandomizeMelodyTrackSet(int32_t lower_octave_span = 0, int32_t upper_octave_span = 0);

    /**
     * @brief Realign internal step cursor to an external step.
     * @param step Step number to align to.
     */
    void RealignTo(int32_t step);

    /**
     * @brief Whether the selected track triggers on the current step.
     */
    bool IsNowTrigger(Track track) const
    {
        return now_trigger_[track];
    }

    /**
     * @brief Current absolute melody note for a track (base note + melody offset).
     */
    int32_t GetCurrentMelodyNote(Track track) const
    {
        // After Reset() there is no current step until the first tick, so read the first one
        const size_t step = current_step_ < 0 ? 0u : static_cast<size_t>(current_step_);
        return GetBaseNote() +
               current_melody_track_set_[track][step];
    }

    /**
     * @brief Current transport step index.
     */
    int32_t GetStep() const
    {
        return current_step_;
    }

    /**
     * @brief Active scale used for quantization and randomized note generation.
     */
    const Scale &GetCurrentScale() const
    {
        return kScales[scale_index_].scale;
    }

    /**
     * @brief Get the color associated with the currently selected scale.
     */
    Color GetCurrentScaleColor() const
    {
        return kScales[scale_index_].color;
    }

    /**
     * @brief Current MIDI base note after root and octave offsets are applied.
     */
    int32_t GetBaseNote() const
    {
        return kBaseNote +
               root_note_ +
               octave_offset_ * 12;
    }

    /**
     * @brief Set chromatic root note offset in semitones [0..11].
     */
    void SetRootNote(size_t root_note)
    {
        root_note_ = constrain(root_note, 0u, kRootNotes - 1);
    }

    /**
     * @brief Get current chromatic root note offset in semitones [0..11].
     */
    size_t GetRootNote() const
    {
        return root_note_;
    }

    /**
     * @brief Get the color associated with the current root note.
     */
    Color GetCurrentRootColor() const
    {
        return kRootColors[root_note_];
    }

    /**
     * @brief Set octave transposition offset.
     * @param octave_offset Octave offset constrained to [kMinOctaveOffset..kMaxOctaveOffset].
     */
    void SetOctaveOffset(int32_t octave_offset)
    {
        octave_offset_ = constrain(octave_offset, kMinOctaveOffset, kMaxOctaveOffset);
    }

    /**
     * @brief Get current octave transposition offset.
     */
    int32_t GetOctaveOffset() const
    {
        return octave_offset_;
    }

    /**
     * @brief Get the color associated with the current octave offset.
     */
    Color GetCurrentOctaveColor() const
    {
        for (const auto &[offset, color] : kOctaveColors)
        {
            if (offset == octave_offset_)
            {
                return color;
            }
        }
        return Color::BLACK;
    }

private:
    /**
     * @brief Quantize all notes in a melody track set to a scale.
     */
    static void QuantizeTrackSetToScale(MelodyTrackSet &track_set, const Scale &scale);

    /**
     * @brief Current step cursor; initialized so first tick lands on step 0.
     */
    int32_t current_step_ = -1;

    /**
     * @brief Active step count (can be reduced in future if variable-length patterns are added).
     */
    size_t steps_ = kSequencerMaxLength;

    /**
     * @brief Current rhythm pattern selection.
     */
    RhythmTrackSet current_rhythm_track_set_ = kRhythmTrackSets[0];

    /**
     * @brief Current melody pattern content (possibly quantized/randomized).
     */
    MelodyTrackSet current_melody_track_set_ = kMelodyTrackSets[0];

    /**
     * @brief Chromatic root note offset [0..11].
     */
    size_t root_note_ = 0;

    /**
     * @brief Octave transposition offset.
     */
    int32_t octave_offset_ = 0;

    /**
     * @brief Selected scale index into kScales.
     */
    size_t scale_index_ = kChromaticScaleIndex;

    /**
     * @brief Last selected melody pattern index, or -1 when melody was randomized.
     */
    int32_t prev_melody_track_set_index_ = -1;

    /**
     * @brief Per-track trigger state for the current process call.
     */
    EnumArray<Track, bool> now_trigger_;
};

}