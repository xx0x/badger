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

#include <cstdint>

namespace badger
{

/**
 * @brief Basic colors in RGB hex format.
 * @details
 * Our hardware uses just one bit per color (1 for on, 0 for off), but the hex values provide
 * a more standard representation of colors and compatibility with possible hardware upgrades in the future.
 * @author Vaclav Mach
 * @date 2026-06-02
 */
enum class Color : uint32_t
{
    BLACK = 0x000000U,
    WHITE = 0xDDDDDDU,
    RED = 0xFF0000U,
    GREEN = 0x00FF00U,
    BLUE = 0x0000FFU,
    YELLOW = 0xFFFF00U,
    CYAN = 0x00FFFFU,
    MAGENTA = 0xFF00AAU
};

static constexpr Color ApplyBrightness(Color color, uint8_t brightness)
{
    uint8_t r = ((static_cast<uint32_t>(color) >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((static_cast<uint32_t>(color) >> 8) & 0xFF) * brightness / 255;
    uint8_t b = (static_cast<uint32_t>(color) & 0xFF) * brightness / 255;
    return static_cast<Color>((r << 16) | (g << 8) | b);
}

static constexpr Color MixColors(Color color_a, Color color_b)
{
    uint16_t r = (((static_cast<uint32_t>(color_a) >> 16) & 0xFF) + ((static_cast<uint32_t>(color_b) >> 16) & 0xFF));
    uint16_t g = (((static_cast<uint32_t>(color_a) >> 8) & 0xFF) + ((static_cast<uint32_t>(color_b) >> 8) & 0xFF));
    uint16_t b = ((static_cast<uint32_t>(color_a) & 0xFF) + (static_cast<uint32_t>(color_b) & 0xFF));
    r = r > 255 ? 255 : r;
    g = g > 255 ? 255 : g;
    b = b > 255 ? 255 : b;
    return static_cast<Color>((r << 16) | (g << 8) | b);
}

} // namespace badger
