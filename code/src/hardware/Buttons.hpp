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

#include "py32f0xx_hal.h"
#include "utils/EnumTools.hpp"

namespace badger
{
/**
 * @brief Button scanner and edge-state tracker for button inputs.
 * @author Vaclav Mach
 * @date 2026-06-02
 */
class Buttons
{
public:
    /**
     * @brief That white things on the Badger.
     */
    enum class Button
    {
        X,
        Y,
        Z,
        COUNT
    };

    /**
     * @brief Initializes the button GPIOs and internal state.
     * @return True if initialization was successful, false otherwise.
     */
    bool Init();

    /**
     * @brief Updates the button states, handles debouncing. Should be called periodically.
     */
    void Update();

    /**
     * @brief Whether button is pressed (held down).
     * @param button The button to check.
     * @return True if the button is currently pressed, false otherwise.
     */
    bool Pressed(Button button) const;

    /**
     * @brief Whether button is pressed (raw state, bypassing debouncing).
     * @param button The button to check.
     * @return True if the button is currently pressed, false otherwise.
     */
    bool PressedRaw(Button button) const;

    /**
     * @brief Whether button was just pressed (edge detection).
     * @param button The button to check.
     * @return True if the button was just pressed, false otherwise.
     */
    bool JustPressed(Button button) const;

    /**
     * @brief Whether button was just released (edge detection).
     * @param button The button to check.
     * @return True if the button was just released, false otherwise.
     */
    bool JustReleased(Button button) const;

    /**
     * @brief Whether any button is currently pressed (held down).
     * @return True if any button is pressed, false otherwise.
     */
    bool AnyPressed() const;

    /**
     * @brief Returns the timestamp (in milliseconds) when the button was last pressed.
     * @param button The button to check.
     * @return Timestamp in milliseconds when the button was last pressed, or 0 if not pressed.
     */
    uint32_t GetPressedTimestamp(Button button) const
    {
        return pressed_timestamp_[button];
    }

    /**
     * @brief Checks if a button has been pressed for at least the specified duration.
     * @param button The button to check.
     * @param duration_ms The duration in milliseconds to check against.
     * @return True if the button has been pressed for at least the specified duration, false otherwise.
     */
    bool PressedOver(Button button, uint32_t duration_ms) const
    {
        if (Pressed(button))
        {
            uint32_t current_time = HAL_GetTick();
            uint32_t pressed_time = pressed_timestamp_[button];
            if (
                current_time >= pressed_time &&
                pressed_time != 0 &&
                (current_time - pressed_time) >= duration_ms)
            {
                return true;
            }
        }
        return false;
    }

private:
    struct Pin
    {
        GPIO_TypeDef *port;
        uint16_t pin;
    };

    static constexpr EnumArray<Button, Pin> kPins = {{{GPIOC, GPIO_PIN_14}, {GPIOC, GPIO_PIN_15}, {GPIOB, GPIO_PIN_12}}};

    /**
     * @brief After a button changes state, further changes are ignored for this long to ride out contact bounce.
     * @details The edge itself is reported straight away, so tap tempo timing doesn't suffer.
     */
    static constexpr uint32_t kDebounceMs = 15;

    EnumArray<Button, bool> pressed_{};
    EnumArray<Button, uint32_t> last_change_time_{};
    EnumArray<Button, uint32_t> pressed_timestamp_{};

    EnumArray<Button, bool> just_pressed_{};
    EnumArray<Button, bool> just_released_{};
};
} // namespace badger
