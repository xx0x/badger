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

#include "TapTempoHandler.hpp"

namespace badger
{

void TapTempoHandler::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    tap_flag_ = false;
    taps_count_ = 0;
    tap_tempo_tick_ = 0;
    tapped_bpm_ = 0.0f;
    tap_tempo_ticks_.Reset();
}

bool TapTempoHandler::Process()
{
    bool tempo_updated = false;

    // If a tap session is active, increment tap_tempo_tick_ too
    if (taps_count_ > 0)
    {
        tap_tempo_tick_++;
    }

    // Process tap events
    if (tap_flag_)
    {
        if (taps_count_ == 0)
        {
            // First tap in this session
            taps_count_ = 1;
            tap_tempo_tick_ = 0;
            tap_tempo_ticks_.Reset();
        }
        else
        {
            // Subsequent taps
            size_t delta_ticks = tap_tempo_tick_;
            size_t max_delay_ticks = static_cast<size_t>(kMaxDelayBetweenTaps * sample_rate_);

            if (delta_ticks > max_delay_ticks)
            {
                // If too long since the previous tap, restart the tap session
                taps_count_ = 1;
                tap_tempo_ticks_.Reset();
            }
            else
            {
                taps_count_++;
                tap_tempo_ticks_.Add(delta_ticks);

                // Only update the tempo if at least two taps have been received
                if (taps_count_ >= 2)
                {
                    size_t avg_ticks = tap_tempo_ticks_.GetAverage();
                    if (avg_ticks > 0)
                    {
                        // One tap interval is a quarter note, so the tempo comes straight out
                        // of the averaged interval.
                        tapped_bpm_ = 60.0f * sample_rate_ / static_cast<float>(avg_ticks);
                        tempo_updated = true;
                    }
                }
            }
            // Reset the tap timing counter after processing a tap
            tap_tempo_tick_ = 0;
        }
        tap_flag_ = false;
    }
    else if (taps_count_ == 1)
    {
        // If a tap session is active but no new tap,
        // check if the delay has exceeded the allowed maximum
        size_t max_delay_ticks = static_cast<size_t>(kMaxDelayBetweenTaps * sample_rate_);
        if (tap_tempo_tick_ > max_delay_ticks)
        {
            // Tap sequence timed out; reset tap session
            taps_count_ = 0;
            tapped_bpm_ = 0.0f;
            tap_tempo_ticks_.Reset();
        }
    }

    return tempo_updated;
}

void TapTempoHandler::TapTempo()
{
    tap_flag_ = true;
}

void TapTempoHandler::ClearTapTempo()
{
    tap_tempo_ticks_.Reset();
    taps_count_ = 0;
    tapped_bpm_ = 0.0f;
    tap_flag_ = false;
}

} // namespace badger