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

#include "hardware/AnalogInputs.hpp"
#include "dsp/math/math_utils.hpp"

namespace badger
{
bool AnalogInputs::Init()
{
    GPIO_InitTypeDef gpioInit{};
    RCC_PeriphCLKInitTypeDef adc_clk_init{};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_ADC_FORCE_RESET();
    __HAL_RCC_ADC_RELEASE_RESET();
    __HAL_RCC_ADC_CLK_ENABLE();

    // The ADC tops out at 16 MHz, so divide the 48 MHz PCLK down to 12 MHz.
    adc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    adc_clk_init.ADCClockSelection = RCC_ADCCLKSOURCE_PCLK_DIV4;
    if (HAL_RCCEx_PeriphCLKConfig(&adc_clk_init) != HAL_OK)
    {
        return false;
    }

    gpioInit.Mode = GPIO_MODE_ANALOG;
    gpioInit.Pull = GPIO_NOPULL;

    gpioInit.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpioInit);

    gpioInit.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOB, &gpioInit);

    // Scan all inputs continuously and let the DMA keep the latest results in dma_buffer_,
    // so reading them never blocks the UI loop (each scan takes 7 * 252 / 12 MHz = ~147 us).
    hadc_.Instance = ADC1;
    hadc_.Init.Resolution = ADC_RESOLUTION_12B;
    hadc_.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc_.Init.ScanConvMode = ADC_SCAN_ENABLE;
    hadc_.Init.ContinuousConvMode = ENABLE;
    hadc_.Init.NbrOfConversion = static_cast<uint32_t>(AnalogInput::COUNT);
    hadc_.Init.DiscontinuousConvMode = DISABLE;
    hadc_.Init.NbrOfDiscConversion = 1U;
    hadc_.Init.ExternalTrigConv = ADC_SOFTWARE_START;

    if (HAL_ADC_Init(&hadc_) != HAL_OK)
    {
        return false;
    }

    // One rank per input, in AnalogInput order, so dma_buffer_ can be indexed by AnalogInput
    ADC_ChannelConfTypeDef sConfig{};
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    uint32_t rank = ADC_REGULAR_RANK_1;
    for (auto input : EnumRange<AnalogInput>())
    {
        sConfig.Channel = kInputs[input].channel;
        sConfig.Rank = rank++;

        if (HAL_ADC_ConfigChannel(&hadc_, &sConfig) != HAL_OK)
        {
            return false;
        }
    }

    if (HAL_ADCEx_Calibration_Start(&hadc_) != HAL_OK)
    {
        return false;
    }

    if (!InitDma())
    {
        return false;
    }

    if (HAL_ADC_Start_DMA(&hadc_,
                          reinterpret_cast<uint32_t *>(const_cast<uint16_t *>(dma_buffer_.data())),
                          static_cast<uint32_t>(dma_buffer_.size())) != HAL_OK)
    {
        return false;
    }

    // HAL_ADC_Start_DMA() enables the DMA interrupts, but nothing needs them: the buffer is just polled.
    // Their IRQ is never enabled in the NVIC either, this only keeps the flags from piling up.
    __HAL_DMA_DISABLE_IT(&hdma_, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE);

    values_.fill(0u);
    frozen_values_.fill(kUnfrozen);
    unfreeze_pending_.fill(false);
    unfreeze_pending_since_.fill(0u);

    // Let the DMA fill all the oversampled scans (~1.2 ms) before the first reading
    HAL_Delay(2);

    // Initial calibration, blocking is fine here as the audio is not running yet.
    // Space the readings out by a millisecond so each one comes from a fresh scan.
    light_sensor_calibrations_.fill(0u);
    CalibrateLightSensors();
    while (light_sensor_calibration_samples_left_ > 0)
    {
        HAL_Delay(1);
        Update();
    }

    return true;
}

bool AnalogInputs::InitDma()
{
    __HAL_RCC_DMA_CLK_ENABLE();

    // DMA1 channel 1 belongs to the audio DAC, the ADC gets channel 2.
    hdma_.Instance = DMA1_Channel2;
    hdma_.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_.Init.MemInc = DMA_MINC_ENABLE;
    hdma_.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_.Init.Mode = DMA_CIRCULAR;
    hdma_.Init.Priority = DMA_PRIORITY_LOW;

    if (HAL_DMA_DeInit(&hdma_) != HAL_OK)
    {
        return false;
    }
    if (HAL_DMA_Init(&hdma_) != HAL_OK)
    {
        return false;
    }

    HAL_DMA_ChannelMap(&hdma_, DMA_CHANNEL_MAP_ADC1);
    __HAL_LINKDMA(&hadc_, DMA_Handle, hdma_);

    return true;
}

bool AnalogInputs::Update()
{
    for (auto input : EnumRange<AnalogInput>())
    {
        // Average the oversampled scans (the DMA keeps overwriting them meanwhile, which only shifts the window)
        uint32_t sum = 0;
        for (size_t scan = 0; scan < kOversampling; ++scan)
        {
            sum += dma_buffer_[scan * kInputCount + static_cast<size_t>(input)];
        }
        uint16_t val = static_cast<uint16_t>((sum + kOversampling / 2) / kOversampling);

        if (kInputs[input].inverted)
        {
            val = kMaxValue - val;
        }
        values_[input] = val;
    }

    ProcessLightSensorCalibration();

    return true;
}

std::uint16_t AnalogInputs::GetRawValue(AnalogInput analog_input) const
{
    return values_[analog_input];
}

void AnalogInputs::FreezePots()
{
    for (auto pot : EnumRange<Pot>())
    {
        FreezePot(pot);
    }
}

bool AnalogInputs::IsPotFrozen(Pot pot) const
{
    AnalogInput analog_input = PotToAnalogInput(pot);
    return frozen_values_[analog_input] != kUnfrozen;
}

bool AnalogInputs::AllPotsFrozen() const
{
    for (auto pot : EnumRange<Pot>())
    {
        if (!IsPotFrozen(pot))
        {
            return false;
        }
    }
    return true;
}

void AnalogInputs::FreezePot(Pot pot)
{
    AnalogInput analog_input = PotToAnalogInput(pot);
    frozen_values_[analog_input] = values_[analog_input];
    unfreeze_pending_[analog_input] = false;
}

std::optional<std::uint16_t> AnalogInputs::ReadPot(Pot pot)
{
    AnalogInput analog_input = PotToAnalogInput(pot);
    auto frozen_value = frozen_values_[analog_input];
    auto current_value = values_[analog_input];

    // Not frozen, return
    if (frozen_value == kUnfrozen)
    {
        return current_value;
    }

    // Try unfreeze: the value has to move significantly and stay moved for a moment,
    // so a burst of noise can't unlock the pot by itself
    if (diff(frozen_value, current_value) > kUnfreezeThreshold)
    {
        const uint32_t now = HAL_GetTick();
        if (!unfreeze_pending_[analog_input])
        {
            unfreeze_pending_[analog_input] = true;
            unfreeze_pending_since_[analog_input] = now;
        }
        else if (now - unfreeze_pending_since_[analog_input] >= kUnfreezeConfirmMs)
        {
            frozen_values_[analog_input] = kUnfrozen;
            unfreeze_pending_[analog_input] = false;
            return current_value;
        }
        return std::nullopt;
    }

    unfreeze_pending_[analog_input] = false;
    return std::nullopt;
}

void AnalogInputs::CalibrateLightSensors()
{
    // Restart the averaging, the readings are collected by the following Update() calls
    light_sensor_calibration_sums_.fill(0u);
    light_sensor_calibration_samples_left_ = kLightSensorCalibrationSamples;
}

void AnalogInputs::ProcessLightSensorCalibration()
{
    if (light_sensor_calibration_samples_left_ == 0)
    {
        return;
    }

    for (auto light_sensor : EnumRange<LightSensor>())
    {
        light_sensor_calibration_sums_[light_sensor] += GetRawValue(LightSensorToAnalogInput(light_sensor));
    }

    if (--light_sensor_calibration_samples_left_ == 0)
    {
        for (auto light_sensor : EnumRange<LightSensor>())
        {
            light_sensor_calibrations_[light_sensor] =
                static_cast<std::uint16_t>(light_sensor_calibration_sums_[light_sensor] / kLightSensorCalibrationSamples);
        }
    }
}

uint32_t AnalogInputs::GatherEntropy() const
{
    uint32_t seed = HAL_GetTick();
    seed ^= SysTick->VAL;

    // Mixes in the individual scans rather than the averaged values, the noise in their low bits is the point
    for (size_t i = 0; i < dma_buffer_.size(); ++i)
    {
        seed = (seed << 5) | (seed >> 27);
        seed ^= static_cast<uint32_t>(dma_buffer_[i]);
    }

    return seed;
}

} // namespace badger
