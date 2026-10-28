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

#include "TrackSequencer.hpp"
#include "dsp/math/HyperRandomness.hpp"
#include "dsp/math/math_utils.hpp"

namespace badger
{

void TrackSequencer::Init()
{
    Reset();
}

void TrackSequencer::Reset()
{
    current_step_ = -1; // Start at -1 so that the first call to Process() sets it to 0
}

void TrackSequencer::SetRhythmTrackSet(size_t index)
{
    if (index < kRhythmTrackSetsCount)
    {
        current_rhythm_track_set_ = kRhythmTrackSets[index];
    }
}

void TrackSequencer::SetMelodyTrackSet(size_t index)
{
    if (index < kMelodyTrackSetsCount)
    {
        if (prev_melody_track_set_index_ != static_cast<int32_t>(index))
        {
            prev_melody_track_set_index_ = index;
            current_melody_track_set_ = kMelodyTrackSets[index];
            QuantizeTrackSetToScale(current_melody_track_set_, GetCurrentScale());
        }
    }
}

void TrackSequencer::SetScaleIndex(size_t index)
{
    scale_index_ = constrain(index, 0, kScales.size() - 1);

    // Re-quantize the current melody track set to the new scale (if it was previously set not randomized)
    if (prev_melody_track_set_index_ != -1)
    {
        current_melody_track_set_ = kMelodyTrackSets[prev_melody_track_set_index_];
    }
    QuantizeTrackSetToScale(current_melody_track_set_, GetCurrentScale());
}

void TrackSequencer::RandomizeRhythmTrackSet()
{
    for (auto track : EnumRange<Track>())
    {
        current_rhythm_track_set_[track] = HyperRandomness::NextU16();
    }
}

void TrackSequencer::RandomizeMelodyTrackSet(int32_t lower_octave_span, int32_t upper_octave_span)
{
    // Force re-quantization
    prev_melody_track_set_index_ = -1;
    const auto rng = []() {
        return HyperRandomness::NextU32();
    };

    // If chromatic scale is selected, use a random scale from the randomizer set to add more variety to the generated melodies
    // Never use chromatic scale for randomization as it would produce too chaotic results
    const Scale &scale = (scale_index_ == kChromaticScaleIndex)
                             ? kRandomizerScales[HyperRandomness::NextU32() % kRandomizerScales.size()]
                             : GetCurrentScale();

    for (auto track : EnumRange<Track>())
    {
        for (size_t step = 0; step < kSequencerMaxLength; ++step)
        {
            current_melody_track_set_[track][step] = scale.RandomNote(rng, lower_octave_span, upper_octave_span);
        }
    }
}

void TrackSequencer::RealignTo(int32_t step)
{
    int32_t normalized = step % static_cast<int32_t>(steps_);
    if (normalized < 0)
    {
        normalized += static_cast<int32_t>(steps_);
    }

    current_step_ = normalized;
    now_trigger_.fill(false);
}

void TrackSequencer::Process(bool clock_tick)
{
    // Reset triggers at the start of each process call
    now_trigger_.fill(false);

    if (clock_tick)
    {
        current_step_ = (current_step_ + 1) % steps_;

        // Update now_trigger_ based on the current rhythm track set and step
        for (auto track : EnumRange<Track>())
        {
            now_trigger_[track] = (current_rhythm_track_set_[track] >> (steps_ - 1 - current_step_)) & 0x1;
        }
    }
}

void TrackSequencer::QuantizeTrackSetToScale(MelodyTrackSet &track_set, const Scale &scale)
{
    for (auto track : EnumRange<TrackSequencer::Track>())
    {
        for (size_t step = 0; step < TrackSequencer::kSequencerMaxLength; ++step)
        {
            track_set[track][step] = scale.ClosestTo(track_set[track][step]);
        }
    }
}

}