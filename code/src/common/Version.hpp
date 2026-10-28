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
#include <array>
#include <cstddef>
#include "common/Color.hpp"

namespace badger
{

/**
 * @brief Stores firmware version metadata and its associated display color.
 * @author Vaclav Mach
 * @date 2026-07-07
 */
class Version
{
public:
    /**
     * @brief Array of RGB colors corresponding to the LEDs.
     */
    using ColorCode = std::array<Color, 3>;

    /**
     * @brief Constructs a version descriptor.
     * @param version_string Null-terminated version string.
     * @param color_code RGB color code associated with the version.
     */
    constexpr Version(const char *version_string, ColorCode color_code) : version_string_(version_string), color_code_(color_code) {}

     /**
     * @brief Creates version metadata from a compile-time string in `x.x.x` format.
     * @tparam N String literal size (must be 6 including null terminator).
     * @param version_string Version string in `x.x.x` format.
     * @return Parsed Version with colors: 0 = Black/none, 1 = Green, 2 = Blue, 3 = Red, 4 = Yellow, 5 = Magenta, 6 = Cyan
     */
    template <std::size_t N>
    static consteval Version FromString(const char (&version_string)[N])
    {
        if (!IsValidString(version_string))
        {
#if defined(__GNUC__)
            __builtin_trap();
#else
            throw "Invalid version string format. Expected x.x.x with digits 0..6.";
#endif
        }
        return Version(
            version_string,
            {
                DigitToColor(version_string[0]),
                DigitToColor(version_string[2]),
                DigitToColor(version_string[4]),
            });
    }

    /**
     * @brief Gets the stored version string.
     * @return Pointer to the null-terminated version string.
     */
    constexpr const char *GetVersionString() const { return version_string_; }

    /**
     * @brief Gets the color code associated with the version.
     * @return Const reference to the RGB color code.
     */
    constexpr const ColorCode &GetColorCode() const { return color_code_; }

private:

    const char *version_string_;
    ColorCode color_code_;

    template <std::size_t N>
    static consteval bool IsValidString(const char (&version_string)[N])
    {
        if constexpr (N != 6)
        {
            return false;
        }
        return version_string[1] == '.' &&
               version_string[3] == '.' &&
               IsValidDigit(version_string[0]) &&
               IsValidDigit(version_string[2]) &&
               IsValidDigit(version_string[4]);
    }

    static consteval bool IsValidDigit(char digit)
    {
        return digit >= '0' && digit <= '6';
    }

    static consteval Color DigitToColor(char digit)
    {
        switch (digit)
        {
        case '0':
            return Color::BLACK;
        case '1':
            return Color::GREEN;
        case '2':
            return Color::BLUE;
        case '3':
            return Color::RED;
        case '4':
            return Color::YELLOW;
        case '5':
            return Color::MAGENTA;
        case '6':
            return Color::CYAN;
        default:
            return Color::BLACK;
        }
    }
};

} // namespace badger