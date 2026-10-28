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

#include "Clock.hpp"

#include "dsp/math/math_utils.hpp"

namespace badger
{

void Clock::Init(float sample_rate)
{
    for (auto source : EnumRange<ClockSource::Type>())
    {
        clock_sources_[source]->Init(sample_rate);
    }

    tap_tempo_handler_.Init(sample_rate);

    active_source_ = ClockSource::Type::INTERNAL;

    Reset();
}

ClockSource::ProcessResult Clock::Process(const ClockSource::Inputs &inputs)
{
    if (tap_tempo_handler_.Process())
    {
        const float tapped_bpm = tap_tempo_handler_.GetTappedBpm();
        if (tapped_bpm > 0.0f)
        {
            internal_clock_source_.SetBpm(tapped_bpm);
        }
    }

    EnumArray<ClockSource::Type, ClockSource::ProcessResult> results;

    for (auto source : EnumRange<ClockSource::Type>())
    {
        results[source] = clock_sources_[source]->Process(inputs);
    }

    UpdateActiveSource();

    return results[active_source_];
}

void Clock::UpdateActiveSource()
{
    for (auto source : EnumRange<ClockSource::Type>())
    {
        if (clock_sources_[source]->GetState() != ClockSource::State::UNAVAILABLE)
        {
            active_source_ = source;
            return;
        }
    }
    // Fallback just in case all sources are unavailable, which shouldn't happen
    active_source_ = ClockSource::Type::INTERNAL;
}

void Clock::SetExtMultiplier(const Fraction &multiplier)
{
    external_clock_source_.SetExtMultiplier(multiplier);
}

Fraction Clock::GetExtMultiplier() const
{
    return external_clock_source_.GetExtMultiplier();
}

void Clock::SetExtMultiplierIndex(size_t index)
{
    if (index >= kExternalDividerMultiplierOptions.size())
    {
        return;
    }

    SetExtMultiplier(kExternalDividerMultiplierOptions[index]);
}

size_t Clock::GetExtMultiplierIndex() const
{
    const Fraction current = GetExtMultiplier();
    for (size_t i = 0; i < kExternalDividerMultiplierOptions.size(); ++i)
    {
        if (kExternalDividerMultiplierOptions[i].Equal(current))
        {
            return i;
        }
    }

    return SIZE_MAX;
}

void Clock::SetMidiDivider(size_t divider)
{
    midi_clock_source_.SetMidiDivider(divider);
}

size_t Clock::GetMidiDivider() const
{
    return midi_clock_source_.GetMidiDivider();
}

void Clock::SetMidiDividerIndex(size_t index)
{
    if (index >= kMidiDividerOptions.size())
    {
        return;
    }

    SetMidiDivider(kMidiDividerOptions[index]);
}

size_t Clock::GetMidiDividerIndex() const
{
    const size_t current = GetMidiDivider();
    for (size_t i = 0; i < kMidiDividerOptions.size(); ++i)
    {
        if (kMidiDividerOptions[i] == current)
        {
            return i;
        }
    }

    return SIZE_MAX;
}

void Clock::SetFromPot(uint16_t pot_value)
{
    static constexpr uint32_t kPotMax = 4095;

    if (active_source_ == ClockSource::Type::MIDI)
    {
        const uint32_t map_size = static_cast<uint32_t>(kMidiDividerOptions.size() - 1);
        const uint32_t index = map(pot_value, 0, kPotMax, 0, map_size, MapClamp::TRUE, MapSafe::FALSE);
        SetMidiDividerIndex(index);
    }
    else if (active_source_ == ClockSource::Type::EXTERNAL)
    {
        const uint32_t map_size = static_cast<uint32_t>(kExternalDividerMultiplierOptions.size() - 1);
        const uint32_t index = map(pot_value, 0, kPotMax, 0, map_size, MapClamp::TRUE, MapSafe::FALSE);
        SetExtMultiplierIndex(index);
    }
    else if (active_source_ == ClockSource::Type::INTERNAL)
    {
        float target_bpm = curve_map(pot_value, kBpmMap, MapClamp::TRUE, MapSafe::FALSE);
        SetBpm(target_bpm);
    }
}

void Clock::TapTempo()
{
    tap_tempo_handler_.TapTempo();
    if (tap_tempo_handler_.GetTapsCount() > 0)
    {
        internal_clock_source_.RealignToTap();
    }
}

}