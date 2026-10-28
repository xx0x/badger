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

#include "common/Midi.hpp"
#include "common/clocksources/ClockSource.hpp"

#include <cstddef>
#include <cstdint>

namespace badger
{

/**
 * @brief Schedules and emits tempo-gated outgoing MIDI notes for the primary sequencer track.
 */
class MidiNoteSender
{
public:
    /**
     * @brief Gate length as a percentage of one sequencer step.
     */
    static constexpr size_t kPrimaryTrackMidiGatePercent = 50;

    /**
     * @brief Note-on velocity used for generated outgoing notes.
     */
    static constexpr uint8_t kPrimaryTrackMidiVelocity = 100;

    /**
     * @brief Pending messages to be sent from the UI loop.
     */
    struct PendingMessages
    {
        bool send_note_on = false;
        bool send_note_off = false;
        Midi::Message note_on = {};
        Midi::Message note_off = {};
    };

    /**
     * @brief Processes one audio-loop tick and schedules pending note on/off messages.
     * @param clock_result Current clock process result.
     * @param clock_state Current clock state.
     * @param midi_channel Selected MIDI output channel.
     * @param primary_track_trigger True when primary track triggers on current step.
     * @param primary_track_note Current primary track note (expected MIDI range).
     */
    void ProcessPrimaryTrack(const ClockSource::ProcessResult &clock_result,
                             ClockSource::State clock_state,
                             uint8_t midi_channel,
                             bool primary_track_trigger,
                             int32_t primary_track_note);

    /**
     * @brief Drains currently pending note on/off messages.
     */
    PendingMessages ConsumePendingMessages();

private:
    struct PendingPrimaryTrackMidi
    {
        volatile bool note_on = false;
        volatile bool note_off = false;
        volatile uint8_t note_on_note = 0;
        volatile uint8_t note_on_channel = 0;
        volatile uint8_t note_off_note = 0;
        volatile uint8_t note_off_channel = 0;
    };

    void QueuePrimaryTrackNoteOn(uint8_t note, uint8_t channel);
    void QueuePrimaryTrackNoteOff(uint8_t note, uint8_t channel);

    PendingPrimaryTrackMidi pending_primary_track_midi_;

    bool primary_track_note_active_ = false;
    uint8_t primary_track_active_note_ = 0;
    uint8_t primary_track_active_channel_ = 0;
    size_t primary_track_note_off_countdown_ = 0;
    size_t callbacks_since_last_step_ = 1;
    size_t last_step_interval_callbacks_ = 1;
};

} // namespace badger
