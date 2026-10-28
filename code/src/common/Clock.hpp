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

#include "common/TapTempoHandler.hpp"
#include "common/clocksources/ClockSource.hpp"
#include "common/clocksources/ExternalClockSource.hpp"
#include "common/clocksources/InternalClockSource.hpp"
#include "common/clocksources/MidiClockSource.hpp"
#include "dsp/math/Fraction.hpp"
#include "dsp/math/math_utils.hpp"
#include "utils/EnumTools.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace badger
{

/**
 * @brief High-level clock controller that routes between MIDI, external, and internal sources.
 * @details
 * Provides a unified API for transport control, tempo/divider configuration,
 * source selection, and pot/tap-tempo interaction.
 * @author Vaclav Mach
 * @date 2026-06-06
 */
class Clock
{
public:
    /**
     * @brief MIDI pulses-per-step divider options for index-based selection.
     */
    static constexpr std::array<size_t, 5> kMidiDividerOptions = {24, 12, 6, 3, 1};

    /**
     * @brief External sync divider/multiplier options for index-based selection.
     */
    static constexpr std::array<Fraction, 5> kExternalDividerMultiplierOptions = {Fraction{1, 4},
                                                                                  Fraction{1, 2},
                                                                                  Fraction{1, 1},
                                                                                  Fraction{2, 1},
                                                                                  Fraction{4, 1}};

    /**
     * @brief Pot curve map used for internal BPM selection.
     * @details Quarter-note tempo, with a sequencer step being one sixteenth note, so the pot
     *          spans roughly 1 to 16.7 steps per second.
     */
    static constexpr auto kBpmMap = MapDef<float, 5>{
        {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(1.0f)},
        {15.0f, 60.0f, 90.0f, 120.0f, 250.0f}};

    /**
     * @brief Initializes all clock sources.
     * @param sample_rate Audio callback sample rate.
     */
    void Init(float sample_rate);

    /**
     * @brief Processes one timing tick and returns transport/step output flags.
     * @param inputs Incoming MIDI (USB and TRS) and external sync pulses for this tick.
     * @return Process result from the active source.
     */
    ClockSource::ProcessResult Process(const ClockSource::Inputs &inputs = {});

    /**
     * @brief Resets selected source phase/counters.
     * @param source Source to reset, or COUNT to target active source.
     */
    void Reset(ClockSource::Type source = ClockSource::Type::COUNT)
    {
        return GetSource(source)->Reset();
    }

    /**
     * @brief Returns half-duty output of the active source.
     */
    bool GetHalfDutyOutput() const
    {
        return GetActiveSource()->GetHalfDutyOutput();
    }

    /**
     * @brief Stops selected source transport.
     * @param source Source to stop, or COUNT to target active source.
     */
    void Stop(ClockSource::Type source = ClockSource::Type::COUNT)
    {
        return GetSource(source)->Stop();
    }

    /**
     * @brief Starts selected source transport.
     * @param source Source to start, or COUNT to target active source.
     */
    void Start(ClockSource::Type source = ClockSource::Type::COUNT)
    {
        return GetSource(source)->Start();
    }

    /**
     * @brief Continues selected source transport.
     * @param source Source to continue, or COUNT to target active source.
     */
    void Continue(ClockSource::Type source = ClockSource::Type::COUNT)
    {
        return GetSource(source)->Continue();
    }

    /**
     * @brief Starts MIDI transport on behalf of a MIDI port.
     * @param port Port that sent the MIDI Start message.
     * @return True if the port owns the MIDI clock and the transport was started.
     */
    bool MidiStart(MidiClockSource::Port port)
    {
        return midi_clock_source_.Start(port);
    }

    /**
     * @brief Stops MIDI transport on behalf of a MIDI port.
     * @param port Port that sent the MIDI Stop message.
     * @return True if the port owns the MIDI clock and the transport was stopped.
     */
    bool MidiStop(MidiClockSource::Port port)
    {
        return midi_clock_source_.Stop(port);
    }

    /**
     * @brief Continues MIDI transport on behalf of a MIDI port.
     * @param port Port that sent the MIDI Continue message.
     * @return True if the port owns the MIDI clock and the transport was continued.
     */
    bool MidiContinue(MidiClockSource::Port port)
    {
        return midi_clock_source_.Continue(port);
    }

    /**
     * @brief Returns the internal source's pulse period, in the raw form meant for persisting.
     */
    uint32_t GetInternalPulsePeriod() const
    {
        return internal_clock_source_.GetPulsePeriod();
    }

    /**
     * @brief Sets external sync divider/multiplier ratio.
     * @param multiplier External clock ratio.
     */
    void SetExtMultiplier(const Fraction &multiplier);

    /**
     * @brief Gets external sync divider/multiplier ratio.
     */
    Fraction GetExtMultiplier() const;

    /**
     * @brief Sets external ratio using kExternalDividerMultiplierOptions index.
     * @param index Option index.
     */
    void SetExtMultiplierIndex(size_t index);

    /**
     * @brief Gets current external ratio option index.
     */
    size_t GetExtMultiplierIndex() const;

    /**
     * @brief Sets MIDI pulses-per-step divider.
     * @param divider Divider value.
     */
    void SetMidiDivider(size_t divider);

    /**
     * @brief Gets MIDI pulses-per-step divider.
     */
    size_t GetMidiDivider() const;

    /**
     * @brief Sets MIDI divider using kMidiDividerOptions index.
     * @param index Option index.
     */
    void SetMidiDividerIndex(size_t index);

    /**
     * @brief Gets current MIDI divider option index.
     */
    size_t GetMidiDividerIndex() const;

    /**
     * @brief Restores a pulse period from GetInternalPulsePeriod(). Ignores MIDI/external sources.
     * @param period Raw internal-source pulse period.
     */
    void SetInternalPulsePeriod(uint32_t period)
    {
        internal_clock_source_.SetPulsePeriod(period);
    }

    /**
     * @brief Sets internal clock tempo in BPM. Ignores MIDI/external sources.
     * @param bpm Target tempo.
     */
    void SetBpm(float bpm)
    {
        internal_clock_source_.SetBpm(bpm);
    }

    /**
     * @brief Returns currently active source type.
     */
    ClockSource::Type GetActiveSourceType() const
    {
        return active_source_;
    }

    /**
     * @brief Returns pointer to currently active source.
     */
    ClockSource *GetActiveSource() const
    {
        return clock_sources_[active_source_];
    }

    /**
     * @brief Returns transport state of active source.
     */
    ClockSource::State GetState() const
    {
        return GetActiveSource()->GetState();
    }

    /**
     * @brief Maps tempo-related controls from pot value.
     * @param pot_value Raw 12-bit pot value.
     */
    void SetFromPot(uint16_t pot_value);

    /**
     * @brief Registers a tap-tempo event.
     */
    void TapTempo();

    /**
     * @brief Clears tap-tempo history.
     */
    void ClearTapTempo()
    {
        tap_tempo_handler_.ClearTapTempo();
    }

private:
    ClockSource *GetSource(ClockSource::Type source) const
    {
        if (source == ClockSource::Type::COUNT)
        {
            return GetActiveSource();
        }
        return clock_sources_[source];
    }

    void UpdateActiveSource();

    MidiClockSource midi_clock_source_;
    ExternalClockSource external_clock_source_;
    InternalClockSource internal_clock_source_;

    EnumArray<ClockSource::Type, ClockSource *> clock_sources_ = {&midi_clock_source_, &external_clock_source_, &internal_clock_source_};
    ClockSource::Type active_source_ = ClockSource::Type::INTERNAL;

    TapTempoHandler tap_tempo_handler_;
};

}
