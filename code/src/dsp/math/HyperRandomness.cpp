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

#include "dsp/math/HyperRandomness.hpp"

namespace badger
{

uint32_t HyperRandomness::Scramble(uint32_t value)
{
    // MurmurHash3-style finalizer for quick entropy diffusion.
    value ^= value >> 16;
    value *= 0x85EBCA6Bu;
    value ^= value >> 13;
    value *= 0xC2B2AE35u;
    value ^= value >> 16;
    return value;
}

void HyperRandomness::Seed(uint32_t seed)
{
    state_ = Scramble(seed ^ 0xA5A5A5A5u);
    if (state_ == 0u)
    {
        state_ = 0x6D2B79F5u;
    }
    seeded_ = true;
}

void HyperRandomness::AddEntropy(uint32_t entropy)
{
    const uint32_t mixed = Scramble(entropy + 0x9E3779B9u);
    if (!seeded_)
    {
        Seed(mixed);
        return;
    }

    state_ ^= mixed;
    if (state_ == 0u)
    {
        state_ = 0x6D2B79F5u;
    }
}

uint32_t HyperRandomness::NextU32()
{
    if (!seeded_)
    {
        Seed(0x12345678u);
    }

    // xorshift32: tiny and fast for MCU use.
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = (x == 0u) ? 0x6D2B79F5u : x;
    return state_;
}

uint16_t HyperRandomness::NextU16()
{
    return static_cast<uint16_t>(NextU32() & 0xFFFFu);
}

int32_t HyperRandomness::NextRange(int32_t min_inclusive, int32_t max_exclusive)
{
    const int32_t span = max_exclusive - min_inclusive;
    if (span <= 0)
    {
        return min_inclusive;
    }

    const uint32_t value = NextU32() % static_cast<uint32_t>(span);
    return min_inclusive + static_cast<int32_t>(value);
}

float HyperRandomness::NextFloat()
{
    return static_cast<float>(NextU32()) / static_cast<float>(UINT32_MAX);
}

} // namespace badger
