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

#include "common/MidiNoteSender.hpp"

#include "utils/no_interrupts_block.hpp"

namespace badger
{

void MidiNoteSender::ProcessPrimaryTrack(const ClockSource::ProcessResult &clock_result,
                                         ClockSource::State clock_state,
                                         uint8_t midi_channel,
                                         bool primary_track_trigger,
                                         int32_t primary_track_note)
{
    const bool clock_running = clock_state == ClockSource::State::RUNNING;
    const bool midi_output_enabled = midi_channel != Midi::OMNI_CHANNEL;

    if (clock_running)
    {
        callbacks_since_last_step_ = callbacks_since_last_step_ + 1;
    }

    // Ensure no hanging note remains if transport is stopped or MIDI output gets disabled.
    if (primary_track_note_active_ && (!clock_running || !midi_output_enabled))
    {
        QueuePrimaryTrackNoteOff(primary_track_active_note_, primary_track_active_channel_);
        primary_track_note_active_ = false;
        primary_track_note_off_countdown_ = 0;
    }

    if (primary_track_note_active_ && primary_track_note_off_countdown_ > 0)
    {
        primary_track_note_off_countdown_ = primary_track_note_off_countdown_ - 1;
        if (primary_track_note_off_countdown_ == 0)
        {
            QueuePrimaryTrackNoteOff(primary_track_active_note_, primary_track_active_channel_);
            primary_track_note_active_ = false;
        }
    }

    if (!clock_result.step)
    {
        return;
    }

    if (callbacks_since_last_step_ > 0)
    {
        last_step_interval_callbacks_ = callbacks_since_last_step_;
    }
    callbacks_since_last_step_ = 0;

    if (!midi_output_enabled || !primary_track_trigger)
    {
        return;
    }

    if (primary_track_note_active_)
    {
        QueuePrimaryTrackNoteOff(primary_track_active_note_, primary_track_active_channel_);
        primary_track_note_active_ = false;
    }

    if (primary_track_note < 0)
    {
        primary_track_note = 0;
    }
    else if (primary_track_note > 127)
    {
        primary_track_note = 127;
    }

    const uint8_t note = static_cast<uint8_t>(primary_track_note);

    QueuePrimaryTrackNoteOn(note, midi_channel);

    primary_track_note_active_ = true;
    primary_track_active_note_ = note;
    primary_track_active_channel_ = midi_channel;

    size_t gate_callbacks = (last_step_interval_callbacks_ * kPrimaryTrackMidiGatePercent) / 100;
    if (gate_callbacks == 0)
    {
        gate_callbacks = 1;
    }
    primary_track_note_off_countdown_ = gate_callbacks;
}

void MidiNoteSender::QueuePrimaryTrackNoteOn(uint8_t note, uint8_t channel)
{
    no_interrupts_block([&]() {
        pending_primary_track_midi_.note_on_note = note;
        pending_primary_track_midi_.note_on_channel = channel;
        pending_primary_track_midi_.note_on = true;
    });
}

void MidiNoteSender::QueuePrimaryTrackNoteOff(uint8_t note, uint8_t channel)
{
    no_interrupts_block([&]() {
        pending_primary_track_midi_.note_off_note = note;
        pending_primary_track_midi_.note_off_channel = channel;
        pending_primary_track_midi_.note_off = true;
    });
}

MidiNoteSender::PendingMessages MidiNoteSender::ConsumePendingMessages()
{
    PendingMessages pending = {};

    no_interrupts_block([&]() {
        pending.send_note_on = pending_primary_track_midi_.note_on;
        pending.send_note_off = pending_primary_track_midi_.note_off;

        pending.note_on = {Midi::Type::NOTE_ON,
                           pending_primary_track_midi_.note_on_channel,
                           pending_primary_track_midi_.note_on_note,
                           kPrimaryTrackMidiVelocity};
        pending.note_off = {Midi::Type::NOTE_OFF,
                            pending_primary_track_midi_.note_off_channel,
                            pending_primary_track_midi_.note_off_note,
                            0};

        pending_primary_track_midi_.note_on = false;
        pending_primary_track_midi_.note_off = false;
    });

    return pending;
}

} // namespace badger
