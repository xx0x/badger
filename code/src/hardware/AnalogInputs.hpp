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
#include <optional>

#include "dsp/math/math_utils.hpp"
#include "py32f0xx_hal.h"
#include "utils/EnumTools.hpp"

namespace badger
{
/**
 * @brief Handles general analog input acquisition and post-processing for pots and light sensors.
 * @author Vaclav Mach
 * @date 2026-06-07
 */
class AnalogInputs
{
public:
    /**
     * @brief Available light sensor inputs.
     */
    enum class LightSensor
    {
        Q1,
        Q2,
        Q3,
        COUNT
    };

    /**
     * @brief Available potentiometer inputs.
     */
    enum class Pot
    {
        ALPHA,
        BETA,
        GAMMA,
        COUNT
    };

    /**
     * @brief Maximum ADC value for 12-bit conversions.
     */
    static constexpr std::uint16_t kMaxValue = 4095U; // 12-bit ADC

    /**
     * @brief Initialize ADC peripherals and internal state.
     * @return True when initialization succeeds.
     */
    bool Init();

    /**
     * @brief Refresh cached values from the latest scans of all analog inputs.
     * @details The ADC scans all inputs continuously into a DMA buffer, so this never blocks.
     *          Each value is the average of the last kOversampling scans.
     *          Also advances a pending light sensor calibration, see CalibrateLightSensors().
     * @return True when the update cycle succeeds.
     */
    bool Update();

    /**
     * @brief Freeze all potentiometer values at their current readings.
     */
    void FreezePots();

    /**
     * @brief Freeze one potentiometer value at its current reading.
     * @param pot Potentiometer to freeze.
     */
    void FreezePot(Pot pot);

    /**
     * @brief Check whether a potentiometer is currently frozen.
     * @param pot Potentiometer to query.
     * @return True if the potentiometer is frozen.
     */
    bool IsPotFrozen(Pot pot) const;

    /**
     * @brief Check whether all potentiometers are currently frozen.
     * @return True if all potentiometers are frozen.
     */
    bool AllPotsFrozen() const;

    /**
     * @brief Read a potentiometer value with freeze/unfreeze handling.
     * @details A frozen pot unfreezes once it has moved more than kUnfreezeThreshold away from where it
     *          was frozen and stayed there for kUnfreezeConfirmMs.
     * @param pot Potentiometer to read.
     * @return Current value when available, otherwise no value.
     */
    std::optional<std::uint16_t> ReadPot(Pot pot);

    /**
     * @brief Capture the current light levels as calibration baselines, from that LightSensor values are mapped to range [-1, 1] or other.
     * @details Non-blocking: the samples are averaged over the next kLightSensorCalibrationSamples Update() calls,
     *          and the previous calibration stays in use until then.
     */
    void CalibrateLightSensors();

    /**
     * @brief Get the light sensor value as a range between min and max. Can be below min if the sensor is darker than the calibration value.
     * @param light_sensor The light sensor to read.
     * @param min The minimum value of the range.
     * @param max The maximum value of the range.
     * @param clamp Clamp to range [min, max] if true, otherwise can go below min if darker than calibration value.
     * @return The light sensor value as a range.
     */
    template <typename T>
        requires(std::integral<T> || std::floating_point<T>)
    T GetLightSensorValue(LightSensor light_sensor, T min, T max, bool clamp = false) const
    {
        // Get the calibration value for the light sensor
        auto calibration = light_sensor_calibrations_[light_sensor];
        auto value = GetRawValue(LightSensorToAnalogInput(light_sensor));
        return map(value, calibration, kMaxValue, min, max, clamp ? MapClamp::TRUE : MapClamp::FALSE);
    }

    /**
     * @brief Gather entropy from analog noise sources, used for seeding random number generators.
     * @return Collected entropy value.
     */
    uint32_t GatherEntropy() const;

    /**
     * @brief Get the raw CV input ADC value (0-4095). The input has an on-board voltage divider
     *        scaling the external signal down into the MCU's 0-3.3V ADC range.
     */
    uint16_t GetCvInValue() const
    {
        return GetRawValue(AnalogInput::CV_IN);
    }

    /**
     * @brief Get the CV input reading as a pitch offset in semitones (v/oct), extrapolating beyond
     *        the calibrated points and clamping near-zero readings to reject jack noise.
     */
    float GetCvInSemitones() const
    {
        return RawCvToSemitones(GetCvInValue());
    }

private:
    /**
     * @brief Below this raw ADC reading, the CV input is treated as 0V to reject noise picked up
     *        by the unpatched banana jack (roughly 250 mV given the board's 5V-to-3.3V divider).
     */
    static constexpr uint16_t kCvNoiseFloorRaw = 205;

    /**
     * @brief V/oct calibration: two raw ADC readings at known input voltages, mapped to the semitone
     *        offset each represents relative to 0V (standard 1V/octave = 12 semitones/volt), so the
     *        line naturally extrapolates back to ~0 semitones at 0V. Placeholder values assume a
     *        linear 0-5V input divided down to the 0-4095 ADC range (1V = 819, 4V = 3277) - replace
     *        with values measured using a multimeter and a debug probe for precise calibration.
     */
    static constexpr auto kCvCalibrationMap = MapDef<float, 2>{
        {819.0f, 3277.0f},   // raw ADC counts at 1V and 4V
        {12.0f, 48.0f}};     // corresponding semitone offsets from 0V (1 octave and 4 octaves up)

    /**
     * @brief Maps a raw CV input ADC reading to a pitch offset in semitones (v/oct).
     */
    static constexpr float RawCvToSemitones(uint16_t raw_cv_value)
    {
        if (raw_cv_value < kCvNoiseFloorRaw)
        {
            return 0.0f;
        }
        return curve_map(static_cast<float>(raw_cv_value), kCvCalibrationMap, MapClamp::FALSE, MapSafe::FALSE);
    }

    struct InputConfig
    {
        GPIO_TypeDef *port;
        std::uint16_t pin;
        std::uint32_t channel;
        bool inverted;
    };

    enum class AnalogInput
    {
        POT_ALPHA,
        POT_BETA,
        POT_GAMMA,
        LIGHT_SENSOR_Q1,
        LIGHT_SENSOR_Q2,
        LIGHT_SENSOR_Q3,
        CV_IN,
        COUNT
    };

    std::uint16_t GetRawValue(AnalogInput analog_input) const;

    static constexpr EnumArray<AnalogInput, InputConfig> kInputs = {{
        InputConfig{GPIOA, GPIO_PIN_1, ADC_CHANNEL_1, true},
        InputConfig{GPIOA, GPIO_PIN_0, ADC_CHANNEL_0, true},
        InputConfig{GPIOA, GPIO_PIN_3, ADC_CHANNEL_3, true},
        InputConfig{GPIOA, GPIO_PIN_7, ADC_CHANNEL_7, false},
        InputConfig{GPIOB, GPIO_PIN_0, ADC_CHANNEL_8, false},
        InputConfig{GPIOB, GPIO_PIN_1, ADC_CHANNEL_9, false},
        InputConfig{GPIOA, GPIO_PIN_2, ADC_CHANNEL_2, false},
    }};

    /**
     * @brief Number of light sensor readings averaged into one calibration.
     */
    static constexpr size_t kLightSensorCalibrationSamples = 4;

    static constexpr size_t kInputCount = static_cast<size_t>(AnalogInput::COUNT);

    /**
     * @brief Number of consecutive scans the DMA keeps and Update() averages.
     * @details 8 scans span ~1.2 ms: random noise drops by ~2.8x and a single spike is diluted 8x,
     *          while the added latency stays far below anything noticeable, even on the CV input.
     */
    static constexpr size_t kOversampling = 8;

    ADC_HandleTypeDef hadc_{};
    DMA_HandleTypeDef hdma_{};

    // Written continuously by the DMA: kOversampling scans of all inputs, each one in AnalogInput order
    std::array<volatile uint16_t, kInputCount * kOversampling> dma_buffer_{};

    EnumArray<AnalogInput, uint16_t> values_;
    EnumArray<AnalogInput, uint16_t> frozen_values_;

    // Whether a frozen pot is currently past kUnfreezeThreshold, and since when (HAL tick)
    EnumArray<AnalogInput, bool> unfreeze_pending_;
    EnumArray<AnalogInput, uint32_t> unfreeze_pending_since_;

    EnumArray<LightSensor, uint16_t> light_sensor_calibrations_;
    EnumArray<LightSensor, uint32_t> light_sensor_calibration_sums_;
    size_t light_sensor_calibration_samples_left_ = 0;

    bool InitDma();
    void ProcessLightSensorCalibration();

    static constexpr AnalogInput LightSensorToAnalogInput(LightSensor light_sensor)
    {
        // This shouldn't happen
        if (light_sensor == LightSensor::COUNT)
        {
            return AnalogInput::COUNT;
        }

        // Lookup table
        static constexpr EnumArray<LightSensor, AnalogInput> kLookupTable = {
            AnalogInput::LIGHT_SENSOR_Q1,
            AnalogInput::LIGHT_SENSOR_Q2,
            AnalogInput::LIGHT_SENSOR_Q3};
        return kLookupTable[light_sensor];
    }

    static constexpr AnalogInput PotToAnalogInput(Pot pot)
    {
        // This shouldn't happen
        if (pot == Pot::COUNT)
        {
            return AnalogInput::COUNT;
        }

        // Lookup table
        static constexpr EnumArray<Pot, AnalogInput> kLookupTable = {
            AnalogInput::POT_ALPHA,
            AnalogInput::POT_BETA,
            AnalogInput::POT_GAMMA};
        return kLookupTable[pot];
    }

    static constexpr std::uint16_t kUnfreezeThreshold = 4095u / 24u;
    static constexpr std::uint16_t kUnfrozen = 0xffffu;

    /**
     * @brief How long a frozen pot has to stay past kUnfreezeThreshold before it unfreezes, in milliseconds.
     * @details A deliberate turn stays past the threshold, a burst of noise doesn't.
     */
    static constexpr uint32_t kUnfreezeConfirmMs = 10;
};
} // namespace badger
