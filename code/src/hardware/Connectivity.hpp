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

namespace badger
{

/**
 * @brief Simple wrapper for clock and debug signals.
 * @author Vaclav Mach
 * @date 2026-06-04
 */
class Connectivity
{
public:
    /**
     * @brief Initializes the clock and debug pins.
     * @return Always returns true.
     */
    bool Init();

    /**
     * @brief Sets the clock out pin to the specified value.
     * @param value True to set the pin high, false to set it low.
     */
    void SetClockOut(bool value);

    /**
     * @brief Gets the current state of the clock in pin.
     * @return True if the clock in pin is high, false if it is low.
     */
    bool GetClockIn();

    /**
     * @brief Whether a clock in pulse has started since the last call.
     * @details The start of every pulse is caught by a pin interrupt, so even a trigger too short to be
     *          seen by looking at the pin once per audio callback (every 1.33 ms) is never missed.
     */
    bool ConsumeClockInEdge();

    /**
     * @brief Sets the debug pin to the specified value.
     * @param value True to set the pin high, false to set it low.
     */
    void SetDebugPin(bool value);

    /**
     * @brief Clock in pin interrupt entry point.
     */
    static void ClockInIrqHandler();

private:
    static inline volatile bool clock_in_edge_ = false;
};

}