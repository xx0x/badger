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

#include "hardware/Buttons.hpp"

namespace badger
{
bool Buttons::Init()
{
    GPIO_InitTypeDef gpioInit{};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpioInit.Mode = GPIO_MODE_INPUT;
    gpioInit.Pull = GPIO_PULLUP;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;

    // PC14/PC15 double as the OSC32 pins, but no LSE crystal is fitted so they stay plain GPIOs.
    gpioInit.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOC, &gpioInit);

    gpioInit.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOB, &gpioInit);

    // Take over whatever is held at power-up without reporting it as a fresh press
    for (auto button : EnumRange<Button>{})
    {
        pressed_[button] = PressedRaw(button);
        pressed_timestamp_[button] = pressed_[button] ? HAL_GetTick() : 0;
        just_pressed_[button] = false;
        just_released_[button] = false;
    }

    return true;
}

void Buttons::Update()
{
    const uint32_t now = HAL_GetTick();

    for (auto button : EnumRange<Button>{})
    {
        // Single shot states only last until the next update
        just_pressed_[button] = false;
        just_released_[button] = false;

        // Debounce: an edge is taken immediately, the bouncing right after it is ignored
        if (now - last_change_time_[button] < kDebounceMs)
        {
            continue;
        }

        const bool pressed = PressedRaw(button);
        if (pressed == pressed_[button])
        {
            continue;
        }

        pressed_[button] = pressed;
        last_change_time_[button] = now;
        just_pressed_[button] = pressed;
        just_released_[button] = !pressed;

        // Timestamp of the press, zero while released
        pressed_timestamp_[button] = pressed ? now : 0;
    }
}

bool Buttons::Pressed(Button button) const
{
    return pressed_[button];
}

bool Buttons::PressedRaw(Button button) const
{
    return HAL_GPIO_ReadPin(kPins[button].port, kPins[button].pin) == GPIO_PIN_RESET;
}

bool Buttons::AnyPressed() const
{
    for (auto button : EnumRange<Button>{})
    {
        if (pressed_[button])
        {
            return true;
        }
    }
    return false;
}

bool Buttons::JustPressed(Button button) const
{
    return just_pressed_[button];
}

bool Buttons::JustReleased(Button button) const
{
    return just_released_[button];
}

} // namespace badger
