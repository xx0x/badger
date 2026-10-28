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

#include "SimpleHardClipper.hpp"
#include "dsp/math/math_utils.hpp"

namespace badger
{

void SimpleHardClipper::Init()
{
}

q15_t SimpleHardClipper::Process(q15_t input)
{
    int32_t val = input;

    // Should be 15, it's 10 to get some drive
    val = val + ((val * drive_) >> 10);

    // Clip result
    q15_t output = q15_saturate(val);

    // The more drive, the less volume
    if (compensation_enabled_)
    {
        output = q15_mult(output, compensation_);
    }

    // Return value
    return output;
}

void SimpleHardClipper::SetVolumeCompensation(bool compensation_enabled)
{
    compensation_enabled_ = compensation_enabled;
}

void SimpleHardClipper::SetDrive(q15_t drive)
{
    drive_ = drive;
    static constexpr MapDef<q15_t, 4> kCompensationMap = MapDef<q15_t, 4>{
        {q15(0.0f), q15(0.05f), q15(0.2f), q15(1.0f)},
        {q15(1.0f), q15(0.43f), q15(0.41f), q15(0.39f)}};
    compensation_ = curve_map(drive_, kCompensationMap, MapClamp::TRUE, MapSafe::FALSE);
}

q15_t SimpleHardClipper::GetDrive() const
{
    return drive_;
}

}