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

#include "hardware/AudioInterface.hpp"

#include <array>

namespace badger
{
AudioInterface *AudioInterface::instance_ = nullptr;

uint32_t AudioInterface::ToDualDacWord(Sample sample)
{
    const auto [left, right] = sample;

    // Offset binary, then down to the DAC's 12 bits.
    const uint32_t left_value = static_cast<uint32_t>(static_cast<uint16_t>(left) ^ 0x8000U) >> 4U;
    const uint32_t right_value = static_cast<uint32_t>(static_cast<uint16_t>(right) ^ 0x8000U) >> 4U;

    // DHR12RD holds channel 1 in bits [11:0] and channel 2 in bits [27:16].
    return left_value | (right_value << 16U);
}

bool AudioInterface::Init()
{
    instance_ = this;
    callback_ = nullptr;

    // Silence (mid-scale) everywhere, so the very first conversions do not thump.
    previous_sample_ = Sample{0, 0};
    dac_dma_buffer_.fill(ToDualDacWord(Sample{0, 0}));

    return InitPins() && InitTimer() && InitDac() && InitDma();
}

bool AudioInterface::InitPins()
{
    GPIO_InitTypeDef gpio_init{};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PA4 is DAC1 (left), PA5 is DAC2 (right).
    gpio_init.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = kAmplifierShutdownPin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    SetAmplifierEnabled(false);

    return true;
}

bool AudioInterface::InitTimer()
{
    __HAL_RCC_TIM6_CLK_ENABLE();

    // The APB prescaler is 1, so the timers run straight off PCLK.
    const uint32_t timer_clock_hz = HAL_RCC_GetPCLK1Freq();

    sample_timer_.Instance = TIM6;
    sample_timer_.Init.Prescaler = 0u;
    sample_timer_.Init.CounterMode = TIM_COUNTERMODE_UP;
    sample_timer_.Init.Period = (timer_clock_hz / kDacSampleRateHz) - 1U;
    sample_timer_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&sample_timer_) != HAL_OK)
    {
        return false;
    }

    // The update event becomes TRGO, which is what the DAC converts on.
    TIM_MasterConfigTypeDef master_config{};
    master_config.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    return HAL_TIMEx_MasterConfigSynchronization(&sample_timer_, &master_config) == HAL_OK;
}

bool AudioInterface::InitDac()
{
    __HAL_RCC_DAC1_CLK_ENABLE();

    dac_.Instance = DAC1;
    if (HAL_DAC_Init(&dac_) != HAL_OK)
    {
        return false;
    }

    DAC_ChannelConfTypeDef channel_config{};
    channel_config.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
    channel_config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

    if (HAL_DAC_ConfigChannel(&dac_, &channel_config, DAC_CHANNEL_1) != HAL_OK)
    {
        return false;
    }

    return HAL_DAC_ConfigChannel(&dac_, &channel_config, DAC_CHANNEL_2) == HAL_OK;
}

bool AudioInterface::InitDma()
{
    __HAL_RCC_DMA_CLK_ENABLE();

    dac_dma_.Instance = DMA1_Channel1;
    dac_dma_.Init.Direction = DMA_MEMORY_TO_PERIPH;
    dac_dma_.Init.PeriphInc = DMA_PINC_DISABLE;
    dac_dma_.Init.MemInc = DMA_MINC_ENABLE;
    dac_dma_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    dac_dma_.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    dac_dma_.Init.Mode = DMA_CIRCULAR;
    dac_dma_.Init.Priority = DMA_PRIORITY_VERY_HIGH;

    if (HAL_DMA_DeInit(&dac_dma_) != HAL_OK)
    {
        return false;
    }
    if (HAL_DMA_Init(&dac_dma_) != HAL_OK)
    {
        return false;
    }

    // Only channel 1 requests DMA; writing DHR12RD updates both channels at once.
    HAL_DMA_ChannelMap(&dac_dma_, DMA_CHANNEL_MAP_DAC1);
    __HAL_LINKDMA(&dac_, DMA_Handle1, dac_dma_);

    dac_dma_.XferHalfCpltCallback = DmaHalfCompleteCallback;
    dac_dma_.XferCpltCallback = DmaCompleteCallback;
    dac_dma_.XferErrorCallback = DmaErrorCallback;

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1U, 0u);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    return true;
}

bool AudioInterface::StartAudio(Callback callback)
{
    callback_ = callback;

    FillBuffer(0u);
    FillBuffer(1U);

    if (HAL_DMA_Start_IT(&dac_dma_,
                         reinterpret_cast<uint32_t>(dac_dma_buffer_.data()),
                         reinterpret_cast<uint32_t>(&dac_.Instance->DHR12RD),
                         static_cast<uint32_t>(kDmaBufferWords)) != HAL_OK)
    {
        return false;
    }

    // Preload silence so the very first trigger does not latch the register's reset value.
    dac_.Instance->DHR12RD = ToDualDacWord(Sample{0, 0});

    SET_BIT(dac_.Instance->CR, DAC_CR_DMAEN1);
    __HAL_DAC_ENABLE(&dac_, DAC_CHANNEL_1);
    __HAL_DAC_ENABLE(&dac_, DAC_CHANNEL_2);

    if (HAL_TIM_Base_Start(&sample_timer_) != HAL_OK)
    {
        return false;
    }

    SetAmplifierEnabled(true);
    return true;
}

void AudioInterface::SetAmplifierEnabled(bool enabled)
{
    // Shutdown is active low, so driving the pin high runs the amplifier.
    HAL_GPIO_WritePin(GPIOB, kAmplifierShutdownPin, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void AudioInterface::EmergencyStop()
{
    // Without the CPU refilling it, the circular DMA would keep looping the last two blocks as a loud buzz
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;

    // Active low shutdown
    GPIOB->BRR = kAmplifierShutdownPin;
}

float AudioInterface::GetSampleRate() const
{
    return static_cast<float>(kDacSampleRateHz) / static_cast<float>(kCallbackDownscale);
}

float AudioInterface::GetCallbackRate() const
{
    return GetSampleRate() / static_cast<float>(kProcessedSamplesPerDmaHalf);
}

void AudioInterface::FillBuffer(size_t half_index)
{
    std::array<Sample, kProcessedSamplesPerDmaHalf> output_buffer{};

    if (callback_ != nullptr)
    {
        callback_(output_buffer);
    }

    uint32_t *dst = &dac_dma_buffer_[half_index * kDacSamplesPerDmaHalf];

    for (const auto &sample : output_buffer)
    {
        const auto [target_left, target_right] = sample;
        const auto [previous_left, previous_right] = previous_sample_;

        // Ramp from the previous frame to this one instead of repeating it. Duplicating would
        // leave the DAC holding a genuine kCallbackDownscale-times-slower staircase, whose
        // images sit just above the audio band; interpolating pushes them up to the DAC rate.
        for (size_t step = 1U; step <= kCallbackDownscale; ++step)
        {
            const int32_t weight = static_cast<int32_t>(step);
            const int16_t left = static_cast<int16_t>(
                previous_left + (((target_left - previous_left) * weight) / static_cast<int32_t>(kCallbackDownscale)));
            const int16_t right = static_cast<int16_t>(
                previous_right + (((target_right - previous_right) * weight) / static_cast<int32_t>(kCallbackDownscale)));

            *dst++ = ToDualDacWord(Sample{left, right});
        }

        previous_sample_ = sample;
    }
}

void AudioInterface::HandleHalfComplete()
{
    FillBuffer(0u);
}

void AudioInterface::HandleComplete()
{
    FillBuffer(1U);
}

void AudioInterface::HandleDmaError()
{
    // DMA error while streaming audio, halt execution
    __builtin_trap();
}

void AudioInterface::HandleDmaIrq()
{
    HAL_DMA_IRQHandler(&dac_dma_);
}

void AudioInterface::DmaHalfCompleteCallback(DMA_HandleTypeDef *hdma)
{
    if ((instance_ != nullptr) && (hdma == &instance_->dac_dma_))
    {
        instance_->HandleHalfComplete();
    }
}

void AudioInterface::DmaCompleteCallback(DMA_HandleTypeDef *hdma)
{
    if ((instance_ != nullptr) && (hdma == &instance_->dac_dma_))
    {
        instance_->HandleComplete();
    }
}

void AudioInterface::DmaErrorCallback(DMA_HandleTypeDef *hdma)
{
    if ((instance_ != nullptr) && (hdma == &instance_->dac_dma_))
    {
        instance_->HandleDmaError();
    }
}
} // namespace badger

extern "C" void DMA1_Channel1_IRQHandler(void)
{
    if (badger::AudioInterface::instance_ != nullptr)
    {
        badger::AudioInterface::instance_->HandleDmaIrq();
    }
}
