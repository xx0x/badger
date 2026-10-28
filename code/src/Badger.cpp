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

#include "Badger.hpp"
#include "py32f0xx_hal.h"

/**
 * @brief Hard fault handler, Badger::ErrorHandler() ends up here as well.
 * @details Spinning here forever would leave the audio DMA looping the last buffer (a loud buzz until
 *          power off), so the output is silenced and the device restarts instead.
 */
extern "C" void HardFault_Handler(void)
{
    badger::AudioInterface::EmergencyStop();
    NVIC_SystemReset();
}

namespace badger
{

void Badger::Init()
{
    HAL_Init();
    SystemClockConfig<SystemClock::HSE>();

    // Initialize all subsystems and the application
    // If any of these fail, we call the ErrorHandler() to halt execution
    if (!(audio.Init() &&
          analog_inputs.Init() &&
          buttons.Init() &&
          connectivity.Init() &&
          midi.Init() &&
          usb_midi.Init() &&
          leds.Init() &&
          eeprom.Init() &&
          app.Init()))
    {
        ErrorHandler();
    }
}

void Badger::StartAudio()
{
    if (!audio.StartAudio(AudioLoop))
    {
        ErrorHandler();
    }
}

void Badger::UiLoop()
{
    // Read inputs
    buttons.Update();
    analog_inputs.Update();

    // Read MIDI messages
    // TRS first: it forwards its incoming bytes into the USB TX stream, which tud_task() then flushes.
    midi.Update();
    usb_midi.Update();

    // Handle layers
    // Must be called before application UI Loop to freeze pots on layer change and clear button release events
    layers.Update();

    // Application UI logic
    app.UiLoop();

    // Send LEDs
    leds.Update();
}

void Badger::AudioLoop(AudioInterface::Buffer output_buffer)
{
    app.AudioLoop(output_buffer);
}

template <Badger::SystemClock system_clock>
void Badger::SystemClockConfig()
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {};

    if constexpr (system_clock == SystemClock::HSE)
    {
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        RCC_OscInitStruct.HSEState = RCC_HSE_ON;
        RCC_OscInitStruct.HSEFreq = RCC_HSE_16_32MHz;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
    }
    else if constexpr (system_clock == SystemClock::HSI)
    {
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState = RCC_HSI_ON;
        RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
        RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
        RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
    }

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        ErrorHandler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        ErrorHandler();
    }
}

} // namespace badger