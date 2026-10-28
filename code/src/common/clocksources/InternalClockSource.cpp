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

#include "common/clocksources/InternalClockSource.hpp"

#include <algorithm>

namespace badger
{

void InternalClockSource::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    state_ = State::RUNNING;
    SetBpm(kDefaultBpm);
    Reset();
}

void InternalClockSource::Start()
{
    state_ = State::RUNNING;
    pulse_phase_ = 0;
    // Prime phase so the first pulse after START also advances step 0.
    pulse_subcounter_ = static_cast<uint8_t>(kPulseMultiplier - 1);
    half_duty_output_ = true;
    pending_result_.midi_start = true;
}

void InternalClockSource::Stop()
{
    state_ = State::STOPPED;
    half_duty_output_ = false;
    pending_result_.midi_stop = true;
}

void InternalClockSource::Continue()
{
    state_ = State::RUNNING;
    pending_result_.midi_continue = true;
}

void InternalClockSource::Reset()
{
    pulse_phase_ = 0;
    pulse_subcounter_ = 0;
    absolute_step_ = 0;
    half_duty_output_ = true;
    pending_result_ = {};
}

ClockSource::ProcessResult InternalClockSource::Process([[maybe_unused]] const Inputs &inputs)
{
    ProcessResult result = pending_result_;
    pending_result_ = {};

    pulse_phase_ += kTickOne;

    if (pulse_phase_ >= pulse_period_)
    {
        // Keeping the remainder rather than zeroing it is what makes the average tempo exact.
        pulse_phase_ -= pulse_period_;
        result.midi_pulse = true;

        ++pulse_subcounter_;
        if (pulse_subcounter_ >= kPulseMultiplier / 2)
        {
            half_duty_output_ = false;
        }

        if (pulse_subcounter_ >= kPulseMultiplier)
        {
            pulse_subcounter_ = 0;
            result.step = state_ == State::RUNNING;
            if (result.step)
            {
                ++absolute_step_;
            }
            half_duty_output_ = true;
        }
    }

    return result;
}

ClockSource::State InternalClockSource::GetState() const
{
    return state_;
}

bool InternalClockSource::GetHalfDutyOutput() const
{
    return half_duty_output_ && state_ == State::RUNNING;
}

uint32_t InternalClockSource::GetPulsePeriod() const
{
    return pulse_period_;
}

size_t InternalClockSource::GetAbsoluteStep() const
{
    return absolute_step_;
}

uint32_t InternalClockSource::PeriodFromTicks(float ticks)
{
    static constexpr float kMaxPeriodTicks = static_cast<float>(UINT32_MAX - kTickOne) / static_cast<float>(kTickOne);

    const float constrained_ticks = std::clamp(ticks, 1.0f, kMaxPeriodTicks);
    return static_cast<uint32_t>(constrained_ticks * static_cast<float>(kTickOne) + 0.5f);
}

uint32_t InternalClockSource::PeriodFromBpm(float bpm) const
{
    // bpm is a quarter-note tempo, so the pulse rate follows the MIDI resolution, not the number
    // of pulses that happen to make up a step.
    return PeriodFromTicks(sample_rate_ * 60.0f / (bpm * static_cast<float>(kPulsesPerQuarterNote)));
}

void InternalClockSource::ApplyPulsePeriod(uint32_t period)
{
    pulse_period_ = period;

    if (pulse_phase_ >= pulse_period_)
    {
        pulse_phase_ = 0;
    }
}

void InternalClockSource::SetBpm(float bpm)
{
    ApplyPulsePeriod(PeriodFromBpm(std::clamp(bpm, kMinBpm, kMaxBpm)));
}

void InternalClockSource::SetPulsePeriod(uint32_t period)
{
    if (period == 0u)
    {
        return;
    }

    // A period coming from outside still has to obey the tempo limits. A faster tempo is a
    // shorter period, so kMaxBpm gives the lower bound and kMinBpm the upper one. Clamping the
    // period itself instead of round-tripping it through BPM is what keeps a restored tempo
    // bit-identical to the one that was saved.
    ApplyPulsePeriod(std::clamp(period, PeriodFromBpm(kMaxBpm), PeriodFromBpm(kMinBpm)));
}

void InternalClockSource::RealignToTap()
{
    if (pulse_period_ == 0u)
    {
        return;
    }

    // Arm the internal phase so the next process tick emits a step pulse,
    // making the sequencer follow the tap immediately.
    pulse_phase_ = pulse_period_ - kTickOne;
    pulse_subcounter_ = static_cast<uint8_t>(kPulseMultiplier - 1);
    half_duty_output_ = true;
}

}
