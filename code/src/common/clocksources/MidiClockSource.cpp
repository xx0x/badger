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


#include "common/clocksources/MidiClockSource.hpp"

#include <utility>

namespace badger
{

void MidiClockSource::Init(float sample_rate)
{
    timeout_ticks_ = static_cast<size_t>(sample_rate);
    if (timeout_ticks_ == 0)
    {
        timeout_ticks_ = 1;
    }
    state_ = State::UNAVAILABLE;
    active_port_ = Port::COUNT;
    ticks_since_last_event_.fill(SIZE_MAX);
    next_pulse_divider_ = pulse_divider_;
    pulse_divider_counter_ = 0;
    total_midi_pulses_ = 0;
    half_duty_output_ = true;
}

void MidiClockSource::Start()
{
    state_ = State::RUNNING;
    pulse_divider_counter_ = 0;
    total_midi_pulses_ = 0;
    next_pulse_divider_ = pulse_divider_;
    half_duty_output_ = true;

    if (HasActivePort())
    {
        ticks_since_last_event_[active_port_] = 0;
    }
}

void MidiClockSource::Stop()
{
    if (state_ != State::UNAVAILABLE)
    {
        state_ = State::STOPPED;
    }
    half_duty_output_ = false;
}

void MidiClockSource::Continue()
{
    state_ = State::RUNNING;

    if (HasActivePort())
    {
        ticks_since_last_event_[active_port_] = 0;
    }
}

void MidiClockSource::Reset()
{
    pulse_divider_counter_ = 0;
    total_midi_pulses_ = 0;
    next_pulse_divider_ = pulse_divider_;
    half_duty_output_ = true;
}

bool MidiClockSource::Start(Port port)
{
    if (!TryClaimPort(port))
    {
        return false;
    }

    Start();
    return true;
}

bool MidiClockSource::Stop(Port port)
{
    if (!TryClaimPort(port))
    {
        return false;
    }

    Stop();
    return true;
}

bool MidiClockSource::Continue(Port port)
{
    if (!TryClaimPort(port))
    {
        return false;
    }

    Continue();
    return true;
}

bool MidiClockSource::HasActivePort() const
{
    return active_port_ != Port::COUNT;
}

bool MidiClockSource::TryClaimPort(Port port)
{
    if (port == Port::COUNT)
    {
        return false;
    }

    // Ports are declared highest priority first, so a larger index means lower priority.
    if (HasActivePort() && std::to_underlying(port) > std::to_underlying(active_port_))
    {
        return false;
    }

    ticks_since_last_event_[port] = 0;
    active_port_ = port;
    return true;
}

void MidiClockSource::UpdateActivePort()
{
    for (auto port : EnumRange<Port>())
    {
        if (ticks_since_last_event_[port] <= timeout_ticks_)
        {
            active_port_ = port;
            return;
        }
    }

    active_port_ = Port::COUNT;
}

ClockSource::ProcessResult MidiClockSource::Process(const Inputs &inputs)
{
    ProcessResult result;

    const EnumArray<Port, bool> pulse_inputs = {inputs.usb_midi_pulse, inputs.trs_midi_pulse};

    for (auto port : EnumRange<Port>())
    {
        if (ticks_since_last_event_[port] != SIZE_MAX)
        {
            ++ticks_since_last_event_[port];
        }

        if (pulse_inputs[port])
        {
            ticks_since_last_event_[port] = 0;
        }
    }

    UpdateActivePort();

    // Only the owning port advances the clock, so a second port sending its own tempo is ignored.
    const bool midi_pulse_input = HasActivePort() && pulse_inputs[active_port_];

    if (midi_pulse_input)
    {
        // Apply divider changes only on pulse boundaries to avoid mid-cycle phase jumps.
        if (next_pulse_divider_ != pulse_divider_)
        {
            pulse_divider_ = next_pulse_divider_;
            pulse_divider_counter_ = total_midi_pulses_ % pulse_divider_;
            result.realign_to = GetAbsoluteStep();
        }

        if (state_ == State::UNAVAILABLE)
        {
            state_ = State::RUNNING;
            pulse_divider_counter_ = 0;
        }

        if (state_ == State::RUNNING)
        {
            if (pulse_divider_counter_ == 0)
            {
                result.step = true;
            }

            ++pulse_divider_counter_;
            if (pulse_divider_counter_ >= pulse_divider_)
            {
                pulse_divider_counter_ = 0;
                half_duty_output_ = true;
            }

            size_t half_threshold = pulse_divider_ / 2;
            if (half_threshold == 0)
            {
                half_threshold = 1;
            }
            if (pulse_divider_counter_ >= half_threshold)
            {
                half_duty_output_ = false;
            }
        }

        if (total_midi_pulses_ != SIZE_MAX)
        {
            ++total_midi_pulses_;
        }
    }

    if (!HasActivePort())
    {
        state_ = State::UNAVAILABLE;
        pulse_divider_counter_ = 0;
        half_duty_output_ = false;
    }

    return result;
}

ClockSource::State MidiClockSource::GetState() const
{
    return state_;
}

bool MidiClockSource::GetHalfDutyOutput() const
{
    return half_duty_output_ && state_ == State::RUNNING;
}

size_t MidiClockSource::GetAbsoluteStep() const
{
    if (pulse_divider_ == 0)
    {
        return 0;
    }

    return total_midi_pulses_ / pulse_divider_;
}

void MidiClockSource::SetMidiDivider(size_t divider)
{
    if (divider == 0)
    {
        divider = 1;
    }

    next_pulse_divider_ = divider;
}

size_t MidiClockSource::GetMidiDivider() const
{
    return next_pulse_divider_;
}

}
