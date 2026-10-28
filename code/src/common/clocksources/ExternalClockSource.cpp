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

#include "common/clocksources/ExternalClockSource.hpp"

#include <algorithm>
#include <limits>

namespace badger
{

void ExternalClockSource::Init(float sample_rate)
{
    timeout_ticks_ = static_cast<size_t>(sample_rate);
    if (timeout_ticks_ == 0)
    {
        timeout_ticks_ = 1;
    }
    state_ = State::UNAVAILABLE;
    transport_running_ = true;
    ticks_since_last_edge_ = timeout_ticks_ + 1;
    ratio_ = Fraction{1, 1};
    next_ratio_ = ratio_;
    steps_per_edge_ = 1;
    edges_per_step_ = 1;
    edge_phase_counter_ = 0;
    edge_period_ticks_ = 0;
    step_period_ticks_ = 0;
    between_step_ticks_target_ = 0;
    between_step_ticks_counter_ = 0;
    remaining_between_steps_ = 0;
    absolute_step_ = 0;
    half_duty_output_ = false;
    half_duty_high_ticks_remaining_ = 0;
    midi_ticks_target_ = 0;
    midi_ticks_counter_ = 0;
    midi_pulses_remaining_ = 0;
    pending_result_ = {};
}

void ExternalClockSource::Start()
{
    transport_running_ = true;
    if (state_ != State::UNAVAILABLE)
    {
        state_ = State::RUNNING;
    }
    pending_result_.midi_start = true;
}

void ExternalClockSource::Stop()
{
    transport_running_ = false;
    if (state_ != State::UNAVAILABLE)
    {
        state_ = State::STOPPED;
    }

    half_duty_output_ = false;
    half_duty_high_ticks_remaining_ = 0;
    remaining_between_steps_ = 0;
    midi_pulses_remaining_ = 0;
    pending_result_.midi_stop = true;
}

void ExternalClockSource::Continue()
{
    transport_running_ = true;
    if (state_ != State::UNAVAILABLE)
    {
        state_ = State::RUNNING;
    }
    pending_result_.midi_continue = true;
}

void ExternalClockSource::Reset()
{
    edge_phase_counter_ = 0;
    between_step_ticks_counter_ = 0;
    remaining_between_steps_ = 0;
    absolute_step_ = 0;
    half_duty_output_ = false;
    half_duty_high_ticks_remaining_ = 0;
    midi_ticks_counter_ = 0;
    midi_pulses_remaining_ = 0;
    pending_result_ = {};
}

ClockSource::ProcessResult ExternalClockSource::Process(const Inputs &inputs)
{

    ProcessResult result = pending_result_;
    pending_result_ = {};

    if (ticks_since_last_edge_ != SIZE_MAX)
    {
        ++ticks_since_last_edge_;
    }

    if (half_duty_high_ticks_remaining_ > 0)
    {
        --half_duty_high_ticks_remaining_;
        if (half_duty_high_ticks_remaining_ == 0)
        {
            half_duty_output_ = false;
        }
    }

    if (state_ == State::RUNNING && midi_pulses_remaining_ > 0 && midi_ticks_target_ > 0)
    {
        ++midi_ticks_counter_;
        if (midi_ticks_counter_ >= midi_ticks_target_)
        {
            midi_ticks_counter_ = 0;
            result.midi_pulse = true;
            --midi_pulses_remaining_;
        }
    }

    // Edges are latched by the pin interrupt, so even pulses shorter than a process tick are counted
    const bool rising_edge = inputs.external_sync_edge;

    if (rising_edge)
    {
        if (ticks_since_last_edge_ > 0 && ticks_since_last_edge_ <= timeout_ticks_)
        {
            edge_period_ticks_ = ticks_since_last_edge_;
        }

        ticks_since_last_edge_ = 0;

        if (!ratio_.Equal(next_ratio_))
        {
            ratio_ = next_ratio_.Simplify();
            RecomputeRatioMode();
            result.realign_to = static_cast<int32_t>(absolute_step_);
        }

        if (state_ == State::UNAVAILABLE)
        {
            state_ = transport_running_ ? State::RUNNING : State::STOPPED;
        }

        if (steps_per_edge_ > 1)
        {
            if (edge_period_ticks_ > 0)
            {
                between_step_ticks_target_ = edge_period_ticks_ / steps_per_edge_;
                if (between_step_ticks_target_ == 0)
                {
                    between_step_ticks_target_ = 1;
                }
                step_period_ticks_ = between_step_ticks_target_;
            }

            between_step_ticks_counter_ = 0;
            remaining_between_steps_ = steps_per_edge_ - 1;
        }
        else
        {
            between_step_ticks_target_ = 0;
            between_step_ticks_counter_ = 0;
            remaining_between_steps_ = 0;

            if (edge_period_ticks_ > 0)
            {
                if (edges_per_step_ > 1)
                {
                    const size_t safe_limit = std::numeric_limits<size_t>::max() / edges_per_step_;
                    step_period_ticks_ = edge_period_ticks_ > safe_limit ? std::numeric_limits<size_t>::max() : edge_period_ticks_ * edges_per_step_;
                }
                else
                {
                    step_period_ticks_ = edge_period_ticks_;
                }
            }
        }

        bool emit_step_on_edge = false;
        if (state_ == State::RUNNING)
        {
            if (steps_per_edge_ > 1 || edges_per_step_ == 1)
            {
                emit_step_on_edge = true;
            }
            else
            {
                emit_step_on_edge = edge_phase_counter_ == 0;
                ++edge_phase_counter_;
                if (edge_phase_counter_ >= edges_per_step_)
                {
                    edge_phase_counter_ = 0;
                }
            }
        }

        if (emit_step_on_edge)
        {
            EmitStep(result);
        }
    }

    if (ticks_since_last_edge_ > timeout_ticks_)
    {
        state_ = State::UNAVAILABLE;
        edge_period_ticks_ = 0;
        step_period_ticks_ = 0;
        between_step_ticks_target_ = 0;
        between_step_ticks_counter_ = 0;
        remaining_between_steps_ = 0;
        midi_pulses_remaining_ = 0;
        half_duty_output_ = false;
        half_duty_high_ticks_remaining_ = 0;
    }

    if (state_ == State::RUNNING && remaining_between_steps_ > 0 && between_step_ticks_target_ > 0)
    {
        ++between_step_ticks_counter_;
        if (between_step_ticks_counter_ >= between_step_ticks_target_)
        {
            between_step_ticks_counter_ = 0;
            --remaining_between_steps_;
            EmitStep(result);
        }
    }

    return result;
}

ClockSource::State ExternalClockSource::GetState() const
{
    return state_;
}

bool ExternalClockSource::GetHalfDutyOutput() const
{
    return half_duty_output_ && state_ == State::RUNNING;
}

size_t ExternalClockSource::GetAbsoluteStep() const
{
    return absolute_step_;
}

void ExternalClockSource::SetExtMultiplier(const Fraction &multiplier)
{
    Fraction normalized = multiplier;
    if (normalized.n == 0)
    {
        normalized.n = 1;
    }
    if (normalized.d == 0)
    {
        normalized.d = 1;
    }
    if (normalized.n < 0)
    {
        normalized.n = -normalized.n;
    }
    if (normalized.d < 0)
    {
        normalized.d = -normalized.d;
    }

    next_ratio_ = normalized.Simplify();
}

Fraction ExternalClockSource::GetExtMultiplier() const
{
    return next_ratio_;
}

void ExternalClockSource::RecomputeRatioMode()
{
    steps_per_edge_ = 1;
    edges_per_step_ = 1;

    const size_t numerator = static_cast<size_t>(std::max<int32_t>(1, ratio_.n));
    const size_t denominator = static_cast<size_t>(std::max<int32_t>(1, ratio_.d));

    if (numerator >= denominator)
    {
        size_t integer_multiplier = numerator / denominator;
        if (integer_multiplier == 0)
        {
            integer_multiplier = 1;
        }
        steps_per_edge_ = integer_multiplier;
    }
    else
    {
        size_t integer_divider = denominator / numerator;
        if (integer_divider == 0)
        {
            integer_divider = 1;
        }
        edges_per_step_ = integer_divider;
        edge_phase_counter_ = 0;
    }
}

void ExternalClockSource::EmitStep(ProcessResult &result)
{
    if (state_ != State::RUNNING)
    {
        return;
    }

    result.step = true;
    ++absolute_step_;

    const size_t pulse_ticks = std::max<size_t>(1, step_period_ticks_);
    half_duty_output_ = true;
    half_duty_high_ticks_remaining_ = pulse_ticks / 2;
    if (half_duty_high_ticks_remaining_ == 0)
    {
        half_duty_high_ticks_remaining_ = 1;
    }

    result.midi_pulse = true;

    const uint8_t deferred_pulses = static_cast<uint8_t>(kMidiPulseMultiplier - 1);
    const uint8_t max_value = std::numeric_limits<uint8_t>::max();
    if (midi_pulses_remaining_ > static_cast<uint8_t>(max_value - deferred_pulses))
    {
        midi_pulses_remaining_ = max_value;
    }
    else
    {
        midi_pulses_remaining_ = static_cast<uint8_t>(midi_pulses_remaining_ + deferred_pulses);
    }

    midi_ticks_target_ = pulse_ticks / kMidiPulseMultiplier;
    if (midi_ticks_target_ == 0)
    {
        midi_ticks_target_ = 1;
    }
    midi_ticks_counter_ = 0;
}

}
