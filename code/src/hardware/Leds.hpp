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
#include <cstdint>
#include "common/Color.hpp"

#include "py32f0xx_hal.h"

namespace badger
{

/**
 * @brief LED driver abstraction for setting and updating LEDs.
 * @details
 * Three common-cathode RGB LEDs hang off nine plain GPIOs through series resistors, so a channel
 * lights up when its pin is driven HIGH. Every one of those nine pins also happens to sit on a
 * timer compare output, which gives us two ways to drive them - see @ref kUsePwm.
 * @author Vaclav Mach
 * @date 2026-06-02
 */
class Leds
{
public:
    /**
     * @brief Number of LEDs on the device.
     */
    static constexpr size_t kLedCount = 3;

    /**
     * @brief Selects how the LED channels are driven.
     * @details
     * When true, every channel is a timer PWM output and the full 8 bits of each color channel
     * are used as a duty cycle (scaled by @ref kBrightness). When false, the pins are plain
     * push-pull outputs and a channel is simply on or off, thresholded at @ref kOnThreshold8Bit.
     *
     * Both paths are compiled through `if constexpr`, so flipping this flag swaps the whole
     * implementation with no runtime cost - handy for comparing the two on real hardware.
     */
    static constexpr bool kUsePwm = true;

    /**
     * @brief Number of PWM duty steps, i.e. the PWM period in timer ticks (the timers run unprescaled).
     * @details This sets the PWM frequency: 48 MHz / 250 = 192 kHz, exactly 4x the 48 kHz DAC rate.
     *          The audio timer also runs off the same clock (1000 ticks per DAC sample), so every DAC
     *          conversion sees the LED switching at the same phase. Any coupling into the audio then
     *          stays a constant offset instead of turning into an audible beat tone.
     *          Keep this a divisor of the DAC period when changing it.
     * @note Ignored when @ref kUsePwm is false.
     */
    static constexpr std::uint32_t kPwmSteps = 250U;

    /**
     * @brief Global brightness: the PWM duty in steps (out of @ref kPwmSteps) of a fully lit color channel.
     * @details 0 = off, kPwmSteps = fully on. Lower color channel levels are scaled proportionally.
     * @note Ignored when @ref kUsePwm is false, since on/off driving has no brightness control.
     */
    static constexpr std::uint8_t kBrightness = 96U;
    static_assert(kBrightness <= kPwmSteps, "kBrightness is a duty in PWM steps, it cannot exceed kPwmSteps");

    /**
     * @brief Initializes the LED driver stuff.
     * @return True if initialization was successful, false otherwise.
     */
    bool Init();

    /**
     * @brief Sets the color of the LED at the given index using a Color enum value.
     * @param index The index of the LED to set (0-based, 0 is the leftmost LED).
     * @param color The color to set, specified as a Color enum value.
     */
    void Set(size_t index, Color color)
    {
        Set(index, static_cast<uint32_t>(color));
    }

    /**
     * @brief Sets the color of the LED at the given index. The color is specified as a 24-bit RGB value (0xRRGGBB).
     * @param index The index of the LED to set (0-based, 0 is the leftmost LED).
     * @param color The color to set, specified as a 24-bit RGB value (0xRRGGBB). With `kUsePwm` the
     *              full 8 bits per channel become a duty cycle, otherwise each channel is thresholded on/off.
     */
    void Set(size_t index, std::uint32_t color);

    /**
     * @brief Sends the colors data to the actual LEDs.
     */
    void Update();

private:
    /**
     * @brief On/off driving only: a channel counts as lit from this level up (~33% of full scale).
     */
    static constexpr std::uint8_t kOnThreshold8Bit = 85U;

    /**
     * @brief Color channels of a single RGB LED, in the order they are stored in `target_`.
     */
    enum class ColorChannel : size_t
    {
        RED,
        GREEN,
        BLUE,
        COUNT
    };

    static constexpr size_t kChannelCount = static_cast<size_t>(ColorChannel::COUNT);

    /**
     * @brief One LED color channel: the GPIO that drives it and the timer output behind that GPIO.
     */
    struct Channel
    {
        GPIO_TypeDef *port;
        std::uint16_t pin;
        std::uint8_t alternate;  ///< AF that routes `pin` to `timer`/`timer_channel`
        TIM_TypeDef *timer;      ///< Timer owning the compare output on `pin`
        std::uint32_t timer_channel; ///< TIM_CHANNEL_x within `timer`
    };

    /**
     * @brief Pin and timer output for every LED channel, ordered [led][ColorChannel].
     * @details LED 0 is the leftmost one on the panel (D1), LED 2 the rightmost (D3).
     */
    static constexpr std::array<std::array<Channel, kChannelCount>, kLedCount> kChannels = {{
        // D1 (left) - all three channels live on TIM1
        {{
            {GPIOA, GPIO_PIN_9, GPIO_AF2_TIM1, TIM1, TIM_CHANNEL_2},
            {GPIOA, GPIO_PIN_8, GPIO_AF2_TIM1, TIM1, TIM_CHANNEL_1},
            {GPIOA, GPIO_PIN_10, GPIO_AF2_TIM1, TIM1, TIM_CHANNEL_3},
        }},
        // D2 (center)
        {{
            {GPIOB, GPIO_PIN_5, GPIO_AF1_TIM3, TIM3, TIM_CHANNEL_2},
            {GPIOB, GPIO_PIN_11, GPIO_AF2_TIM2, TIM2, TIM_CHANNEL_4},
            {GPIOB, GPIO_PIN_10, GPIO_AF2_TIM2, TIM2, TIM_CHANNEL_3},
        }},
        // D3 (right)
        {{
            {GPIOB, GPIO_PIN_3, GPIO_AF2_TIM2, TIM2, TIM_CHANNEL_2},
            {GPIOA, GPIO_PIN_15, GPIO_AF2_TIM2, TIM2, TIM_CHANNEL_1},
            {GPIOB, GPIO_PIN_4, GPIO_AF1_TIM3, TIM3, TIM_CHANNEL_1},
        }},
    }};

    /**
     * @brief The timers whose compare outputs drive the LED channels.
     */
    static constexpr std::array<TIM_TypeDef *, 3> kTimers = {TIM1, TIM2, TIM3};

    bool InitPins();
    bool InitTimers();

    static void WriteChannel(const Channel &channel, std::uint8_t level);
    static volatile std::uint32_t *CompareRegister(const Channel &channel);

    /**
     * @brief Timer handles, only needed while driving the LEDs with PWM.
     */
    std::array<TIM_HandleTypeDef, kUsePwm ? kTimers.size() : 0> timers_{};

    std::array<std::array<std::uint8_t, kChannelCount>, kLedCount> target_{};
    bool dirty_ = true;
};
} // namespace badger
