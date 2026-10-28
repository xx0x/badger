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

#include "dsp/math/Fraction.hpp"

#include <cstddef>
#include <cstdint>

namespace badger
{

/**
 * @brief Abstract interface for all timing/transport clock sources.
 * @details
 * Implementations provide tempo, run state, pulse generation, and transport
 * events for MIDI, external sync, and internal clocks.
 * @author Vaclav Mach
 * @date 2026-06-18
 */
class ClockSource
{
public:
    /**
     * @brief Identifies a concrete clock source implementation.
     */
    enum class Type : uint8_t
    {
        MIDI,
        EXTERNAL,
        INTERNAL,
        COUNT
    };

    /**
     * @brief Runtime availability and transport state of a clock source.
     */
    enum class State : uint8_t
    {
        UNAVAILABLE,
        STOPPED,
        RUNNING,
        COUNT
    };

    /**
     * @brief Per-process timing inputs handed to every clock source.
     */
    struct Inputs
    {
        bool usb_midi_pulse = false;  ///< Incoming MIDI clock pulse from the USB MIDI port.
        bool trs_midi_pulse = false;  ///< Incoming MIDI clock pulse from the TRS MIDI port.
        bool external_sync_edge = false; ///< An analog clock pulse has started since the previous process tick.
    };

    /**
     * @brief Per-sample/process output flags produced by a clock source.
     */
    struct ProcessResult
    {
        bool step = false;
        bool midi_pulse = false;
        bool midi_start = false;
        bool midi_stop = false;
        bool midi_continue = false;
        int32_t realign_to = -1; ///< Optional step index realignment target, -1 means no realign.
    };

    /**
     * @brief Initializes source timing internals.
     * @param sample_rate Audio callback sample rate.
     */
    virtual void Init(float sample_rate) = 0;

    /**
     * @brief Starts transport from the beginning.
     */
    virtual void Start() = 0;

    /**
     * @brief Stops transport (without resetting phase/counters).
     */
    virtual void Stop() = 0;

    /**
     * @brief Continues transport from the current position.
     */
    virtual void Continue() = 0;

    /**
     * @brief Resets timing phase/counters.
     */
    virtual void Reset() = 0;

    /**
     * @brief Advances source state by one process tick.
     * @param inputs Incoming clock pulses for this tick.
     * @return Process flags for sequencer/MIDI outputs.
     */
    virtual ProcessResult Process(const Inputs &inputs) = 0;

    /**
     * @brief Gets source availability/transport state.
     */
    virtual State GetState() const = 0;

    /**
     * @brief Returns half-duty clock output state for LED/gate indication.
     */
    virtual bool GetHalfDutyOutput() const = 0;

    /**
     * @brief Returns absolute step counter maintained by the source.
     */
    virtual size_t GetAbsoluteStep() const = 0;
};

}
