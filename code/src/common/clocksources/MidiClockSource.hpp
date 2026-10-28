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

#include "common/clocksources/ClockSource.hpp"
#include "utils/EnumTools.hpp"

namespace badger
{

/**
 * @brief MIDI clock source shared by the USB MIDI and TRS MIDI ports.
 * @details Both ports feed one source, so they share a single divider and a single transport
 *          state. Whichever port is currently sending traffic drives the clock; when both are
 *          active USB wins and TRS pulses and transport messages are ignored. A port that goes
 *          quiet for longer than the timeout releases its claim and hands over to the other one.
 * @author Vaclav Mach
 * @date 2026-06-18
 */
class MidiClockSource final : public ClockSource
{
public:
    /**
     * @brief The MIDI ports feeding this source, listed highest priority first.
     */
    enum class Port : uint8_t
    {
        USB,
        TRS,
        COUNT ///< Also used as "no port is currently sending MIDI".
    };

    void Init(float sample_rate) override;
    void Start() override;
    void Stop() override;
    void Continue() override;
    void Reset() override;
    ProcessResult Process(const Inputs &inputs) override;
    State GetState() const override;
    bool GetHalfDutyOutput() const override;
    size_t GetAbsoluteStep() const override;

    /**
     * @brief Starts transport on behalf of a MIDI port.
     * @param port Port that sent the MIDI Start message.
     * @return True if the port owns the clock and the transport was started.
     */
    bool Start(Port port);

    /**
     * @brief Stops transport on behalf of a MIDI port.
     * @param port Port that sent the MIDI Stop message.
     * @return True if the port owns the clock and the transport was stopped.
     */
    bool Stop(Port port);

    /**
     * @brief Continues transport on behalf of a MIDI port.
     * @param port Port that sent the MIDI Continue message.
     * @return True if the port owns the clock and the transport was continued.
     */
    bool Continue(Port port);

    void SetMidiDivider(size_t divider);
    size_t GetMidiDivider() const;

private:
    /**
     * @brief Lets `port` take over the clock unless a higher-priority port is currently active.
     * @return True if the port now owns the clock.
     */
    bool TryClaimPort(Port port);

    /**
     * @brief Re-evaluates which port owns the clock based on how recently each one was heard.
     */
    void UpdateActivePort();

    bool HasActivePort() const;

    State state_ = State::UNAVAILABLE;
    Port active_port_ = Port::COUNT;
    EnumArray<Port, size_t> ticks_since_last_event_ = {SIZE_MAX, SIZE_MAX};
    size_t timeout_ticks_ = 1;
    size_t pulse_divider_ = 6;
    size_t next_pulse_divider_ = 6;
    size_t pulse_divider_counter_ = 0;
    size_t total_midi_pulses_ = 0;
    bool half_duty_output_ = false;
};

}
