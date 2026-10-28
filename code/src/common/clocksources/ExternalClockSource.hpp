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

class ExternalClockSource final : public ClockSource
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

    void SetExtMultiplier(const Fraction &multiplier);
    Fraction GetExtMultiplier() const;

private:
    static constexpr uint8_t kMidiPulseMultiplier = 6;

    void RecomputeRatioMode();
    void EmitStep(ProcessResult &result);

    State state_ = State::UNAVAILABLE;
    bool transport_running_ = true;
    size_t ticks_since_last_edge_ = 0;
    size_t timeout_ticks_ = 1;

    Fraction ratio_ = Fraction{1, 1};
    Fraction next_ratio_ = Fraction{1, 1};
    size_t steps_per_edge_ = 1;
    size_t edges_per_step_ = 1;
    size_t edge_phase_counter_ = 0;

    size_t edge_period_ticks_ = 0;
    size_t step_period_ticks_ = 0;

    size_t between_step_ticks_target_ = 0;
    size_t between_step_ticks_counter_ = 0;
    size_t remaining_between_steps_ = 0;

    size_t absolute_step_ = 0;
    bool half_duty_output_ = false;
    size_t half_duty_high_ticks_remaining_ = 0;

    size_t midi_ticks_target_ = 0;
    size_t midi_ticks_counter_ = 0;
    uint8_t midi_pulses_remaining_ = 0;

    ProcessResult pending_result_;
};

}
