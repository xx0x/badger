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

#include "Connectivity.hpp"
#include "py32f0xx_hal.h"
#include "utils/no_interrupts_block.hpp"

extern "C" void EXTI4_15_IRQHandler(void)
{
    badger::Connectivity::ClockInIrqHandler();
}

namespace badger
{

bool Connectivity::Init()
{
    GPIO_InitTypeDef GPIO_InitStruct = {};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    // Clock out pin
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Debug pin, broken out on its own header
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // Clock in pin, inverted by the input transistor, so a pulse starts with a falling edge on the pin
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Below the MIDI UART (0) and the audio DMA (1), the edge stays latched until it is serviced anyway
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

    SetClockOut(false);
    SetDebugPin(false);

    return true;
}

void Connectivity::ClockInIrqHandler()
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_13) != 0u)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);
        clock_in_edge_ = true;
    }
}

bool Connectivity::ConsumeClockInEdge()
{
    // Read and clear in one go, so an edge arriving in between can't get lost
    return no_interrupts_block([]() {
        const bool edge = clock_in_edge_;
        clock_in_edge_ = false;
        return edge;
    });
}

void Connectivity::SetClockOut(bool value)
{
    // Clock out is inverted + buffered + pushed to 5V logic (using 74AHCT1G14)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, value ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool Connectivity::GetClockIn()
{
    // Inverted by the input transistor + pulled up, so active state is when pin reads LOW.
    return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET;
}

void Connectivity::SetDebugPin(bool value)
{
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


}
