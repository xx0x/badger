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

#include "hardware/Leds.hpp"

namespace badger
{

bool Leds::Init()
{
    target_.fill({0u, 0u, 0u});
    dirty_ = true;

    if (!InitPins())
    {
        return false;
    }

    if constexpr (kUsePwm)
    {
        if (!InitTimers())
        {
            return false;
        }
    }

    Update();
    return true;
}

void Leds::Set(size_t index, std::uint32_t color)
{
    if (index >= kLedCount)
    {
        return;
    }

    std::array<std::uint8_t, kChannelCount> next{
        static_cast<std::uint8_t>((color >> 16) & 0xFFU),
        static_cast<std::uint8_t>((color >> 8) & 0xFFU),
        static_cast<std::uint8_t>(color & 0xFFU)};

    if constexpr (!kUsePwm)
    {
        // Without PWM a channel is either on or off, so collapse it right away and only mark the
        // LED dirty when that binary state actually changes.
        for (auto &channel : next)
        {
            channel = (channel >= kOnThreshold8Bit) ? 1U : 0u;
        }
    }

    if (next == target_[index])
    {
        return;
    }

    target_[index] = next;
    dirty_ = true;
}

bool Leds::InitPins()
{
    GPIO_InitTypeDef gpio_init{};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Pull = GPIO_NOPULL;
    // Slow edges switch the LED current more gently, so less of it couples into the audio.
    // Note: the shortest PWM pulse is a single 48 MHz tick (~21 ns). If the dimmest LED levels stop
    // lighting up (or fade unevenly), the edges are too slow for that and this must go back to a faster speed.
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    for (const auto &led : kChannels)
    {
        for (const auto &channel : led)
        {
            gpio_init.Pin = channel.pin;

            if constexpr (kUsePwm)
            {
                gpio_init.Mode = GPIO_MODE_AF_PP;
                gpio_init.Alternate = channel.alternate;
            }
            else
            {
                gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
                gpio_init.Alternate = 0u;
            }

            HAL_GPIO_Init(channel.port, &gpio_init);

            if constexpr (!kUsePwm)
            {
                // Common cathode, so LOW is off.
                HAL_GPIO_WritePin(channel.port, channel.pin, GPIO_PIN_RESET);
            }
        }
    }

    return true;
}

bool Leds::InitTimers()
{
    // The APB prescaler is 1, so the timers run straight off PCLK, and they are not prescaled
    // any further: one PWM period is exactly kPwmSteps PCLK ticks (see kPwmSteps).
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    // Iterating over the (empty in the on/off build) handle array keeps this loop in bounds either way.
    for (size_t i = 0; i < timers_.size(); ++i)
    {
        TIM_HandleTypeDef &timer = timers_[i];

        timer.Instance = kTimers[i];
        timer.Init.Prescaler = 0u;
        timer.Init.CounterMode = TIM_COUNTERMODE_UP;
        timer.Init.Period = kPwmSteps - 1U;
        timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        timer.Init.RepetitionCounter = 0u;
        timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

        if (HAL_TIM_PWM_Init(&timer) != HAL_OK)
        {
            return false;
        }
    }

    TIM_OC_InitTypeDef oc_init{};
    oc_init.OCMode = TIM_OCMODE_PWM1;
    oc_init.Pulse = 0u;
    oc_init.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc_init.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc_init.OCFastMode = TIM_OCFAST_DISABLE;
    oc_init.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc_init.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    for (const auto &led : kChannels)
    {
        for (const auto &channel : led)
        {
            TIM_HandleTypeDef *timer = nullptr;
            for (size_t i = 0; i < timers_.size(); ++i)
            {
                if (kTimers[i] == channel.timer)
                {
                    timer = &timers_[i];
                    break;
                }
            }

            if (timer == nullptr)
            {
                return false;
            }

            if (HAL_TIM_PWM_ConfigChannel(timer, &oc_init, channel.timer_channel) != HAL_OK)
            {
                return false;
            }

            if (HAL_TIM_PWM_Start(timer, channel.timer_channel) != HAL_OK)
            {
                return false;
            }
        }
    }

    return true;
}

volatile std::uint32_t *Leds::CompareRegister(const Channel &channel)
{
    switch (channel.timer_channel)
    {
    case TIM_CHANNEL_1:
        return &channel.timer->CCR1;
    case TIM_CHANNEL_2:
        return &channel.timer->CCR2;
    case TIM_CHANNEL_3:
        return &channel.timer->CCR3;
    case TIM_CHANNEL_4:
        return &channel.timer->CCR4;
    default:
        return nullptr;
    }
}

void Leds::WriteChannel(const Channel &channel, std::uint8_t level)
{
    if constexpr (kUsePwm)
    {
        // level * kBrightness / 255, so a full channel lands exactly on kBrightness steps.
        // At kBrightness == kPwmSteps the compare value exceeds the period and the output stays on.
        const std::uint32_t duty = (static_cast<std::uint32_t>(level) * kBrightness + 127U) / 255U;

        if (volatile std::uint32_t *compare = CompareRegister(channel); compare != nullptr)
        {
            *compare = duty;
        }
    }
    else
    {
        HAL_GPIO_WritePin(channel.port, channel.pin, (level != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void Leds::Update()
{
    if (!dirty_)
    {
        return;
    }

    for (size_t index = 0; index < kLedCount; ++index)
    {
        for (size_t channel = 0; channel < kChannelCount; ++channel)
        {
            WriteChannel(kChannels[index][channel], target_[index][channel]);
        }
    }

    dirty_ = false;
}
} // namespace badger
