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

namespace badger
{

class InternalClockSource final : public ClockSource
{
public:
    void Init(float sample_rate) override;
    void Start() override;
    void Stop() override;
    void Continue() override;
    void Reset() override;
    ProcessResult Process(const Inputs &inputs) override;
    State GetState() const override;
    bool GetHalfDutyOutput() const override;
    size_t GetAbsoluteStep() const override;

    void SetBpm(float bpm);
    void RealignToTap();

    /**
     * @brief Returns the raw pulse period, as stored: Q16.16 audio callbacks per MIDI pulse.
     * @details This is the clock's native tempo representation, exact to the fraction of a
     *          callback. It is what gets persisted, so that a tempo survives a power cycle
     *          unchanged - rounding it to whole callbacks first would shift the tempo by up to
     *          several percent at the fast end of the range.
     * @note The value is relative to the audio callback rate, so it only means the same tempo
     *       on a build whose callback rate matches the one that produced it.
     */
    uint32_t GetPulsePeriod() const;

    /**
     * @brief Restores a pulse period previously obtained from GetPulsePeriod().
     * @param period Q16.16 callbacks per MIDI pulse; ignored when zero, clamped to the tempo
     *               range otherwise.
     */
    void SetPulsePeriod(uint32_t period);

    /**
     * @brief Tempo the clock starts up with, in quarter-note BPM.
     */
    static constexpr float kDefaultBpm = 90.0f;

    /**
     * @brief Lowest and highest tempo the internal clock accepts, in quarter-note BPM.
     * @note Applied to tapped tempos as well, so a tap outside the range is pulled back in.
     */
    static constexpr float kMinBpm = 15.0f;
    static constexpr float kMaxBpm = 250.0f;

private:
    /**
     * @brief Resolution of the MIDI clock, in pulses per quarter note.
     * @details This is what a "pulse" means everywhere in this class: the pulse train emitted by
     *          Process() is a real 24 PPQN MIDI clock, and the pulse period is the number of
     *          audio callbacks between two of those pulses.
     */
    static constexpr size_t kPulsesPerQuarterNote = 24;

    /**
     * @brief MIDI pulses per sequencer step; 6 of 24 PPQN makes a step one sixteenth note.
     * @note This is pulses per *step*, not per quarter note - the two must not be confused, or
     *       the tempo comes out wrong by kPulsesPerQuarterNote / kPulseMultiplier.
     */
    static constexpr uint8_t kPulseMultiplier = 6;

    /**
     * @brief Fixed-point format of the pulse phase accumulator.
     * @details The pulse period is rarely a whole number of audio callbacks, so it is kept as
     *          Q16.16 ticks and accumulated fractionally. Truncating it to an integer instead
     *          costs up to 25% of tempo error at the top of the range, where a pulse is only a
     *          few callbacks long.
     */
    static constexpr uint32_t kTickFractionBits = 16;
    static constexpr uint32_t kTickOne = 1u << kTickFractionBits;

    /**
     * @brief Converts a possibly fractional number of callbacks into a representable period.
     * @param ticks Callbacks per MIDI pulse; clamped to a non-zero, in-range period.
     */
    static uint32_t PeriodFromTicks(float ticks);

    /**
     * @brief Pulse period that yields the given quarter-note tempo at the current sample rate.
     */
    uint32_t PeriodFromBpm(float bpm) const;

    /**
     * @brief Stores a new pulse period, keeping the running phase inside it.
     */
    void ApplyPulsePeriod(uint32_t period);

    float sample_rate_ = 0.0f;
    uint32_t pulse_period_ = kTickOne;
    uint32_t pulse_phase_ = 0;
    uint8_t pulse_subcounter_ = 0;
    size_t absolute_step_ = 0;
    bool half_duty_output_ = false;
    State state_ = State::RUNNING;

    ProcessResult pending_result_;
};

}
