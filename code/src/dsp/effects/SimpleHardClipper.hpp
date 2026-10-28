/*
MIT License

Copyright (c) 2024 Vaclav Mach (Bastl Instruments)

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

#include "dsp/math/qmath.hpp"

namespace badger
{

/**
 * @class SimpleHardClipper
 * @brief Hard clipping overdrive.
 * @author Vaclav Mach (Bastl Instruments)
 * @date 2024-04-24
 */
class SimpleHardClipper
{

public:
    /**
     * @brief Initializes the SimpleHardClipper object.
     */
    void Init();

    /**
     * @brief Enables or disables volume compensation for the hard clipping effect.
     *
     * @param compensation If true, the volume will be lowered at higher drive values to prevent excessive output levels. If false, the volume will not be adjusted based on the drive level.
     */
    void SetVolumeCompensation(bool compensation);

    /**
     * @brief Processes an input sample and applies the hard clipping.
     *
     * @param input The input sample to be processed.
     * @return The processed output sample.
     */
    q15_t Process(q15_t input);

    /**
     * @brief Sets the drive level of the hard clipping.
     *
     * @param drive The drive level to be set. (O-Q15_MAX)
     */
    void SetDrive(q15_t drive);

    /**
     * @brief Gets the current drive level of the hard clipping.
     *
     * @return The current drive level.
     */
    q15_t GetDrive() const;

private:
    q15_t drive_ = 0;
    q15_t compensation_ = q15(1.0f);
    bool compensation_enabled_ = true;
};
}
