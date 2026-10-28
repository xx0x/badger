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
#include <span>
#include <tuple>

#include "py32f0xx_hal.h"

extern "C" void DMA1_Channel1_IRQHandler(void);

namespace badger
{
/**
 * @brief DAC/DMA based audio output interface.
 *
 * AudioInterface feeds signed 16-bit stereo synthesis frames to the two on-chip DAC channels,
 * which drive the left and right inputs of the headphone amplifier.
 *
 * Implementation summary:
 * - Input sample format: stereo int16_t frames (callback writes @ref Sample values into a block).
 * - Conversion: both channels of a frame are offset-binary shifted into the DAC's 12-bit range
 *   and packed into the dual holding register DHR12RD, so one 32-bit word carries the pair.
 * - Pacing: TIM6 runs at exactly kDacSampleRateHz and its update event (TRGO) triggers both
 *   DAC channels, so the output rate is independent of any CPU load.
 * - Transport: a single DMA channel in circular mode moves words into DHR12RD. Channel 1's
 *   DMA request drives it and loading DHR12RD updates both channels at once, so one DMA
 *   channel is enough for the stereo pair.
 * - Buffering: ping-pong DMA buffer (2 halves), the half the DAC is not reading is refilled
 *   from the half/full transfer interrupts.
 * - Callback decimation/interpolation:
 *   - kCallbackDownscale = 2, so the callback renders half as many samples as the DAC consumes
 *     and the gaps are filled by linearly interpolating towards the next rendered frame.
 *     Repeating the frame instead would leave a real 24 kHz staircase on the output, whose
 *     images sit just above the audio band - exactly what an analog reconstruction filter
 *     cannot remove without also eating the treble.
 *   - This allows more complex audio processing at still reasonable sample rate, and keeps the
 *     buffer layout ready for a 48 kHz USB audio path later on.
 *
 * Resulting rates:
 * - DAC output rate: 48 kHz
 * - Effective sample rate seen by the application: 24 kHz
 * - Callback rate: 24000 / 32 = 750 Hz
 *
 * @date 2026-06-03
 * @author Vaclav Mach
 */
class AudioInterface
{
public:
    /**
     * @brief One stereo PCM frame: left channel first, right channel second.
     * @note Left goes to DAC channel 1 (PA4), right to DAC channel 2 (PA5).
     */
    using Sample = std::tuple<int16_t, int16_t>;

    /**
     * @brief Block of stereo frames handed to the render callback.
     */
    using Buffer = std::span<Sample>;

    /**
     * @brief Audio render callback.
     *
     * The callback is invoked for each DMA buffer refill and must write
     * the next block of stereo PCM frames into @p output_buffer.
     */
    using Callback = void (*)(Buffer output_buffer);

    /**
     * @brief Rate at which the DAC converts samples, driven by TIM6.
     * @details Kept at 48 kHz so the buffer layout matches a future USB audio endpoint.
     */
    static constexpr uint32_t kDacSampleRateHz = 48000U;

    /**
     * @brief Number of DAC conversions the interface emits per audio frame the application renders.
     */
    static constexpr uint32_t kCallbackDownscale = 2U;

    /**
     * @brief Effective PCM sample rate the application renders at, the compile-time twin of GetSampleRate().
     */
    static constexpr uint32_t kSampleRateHz = kDacSampleRateHz / kCallbackDownscale;

    /**
     * @brief Initialize GPIO, DAC, timer, DMA and interrupt routing for audio output.
     * @return True if initialization was successful, false otherwise.
     */
    bool Init();

    /**
     * @brief Start continuous audio conversion.
     * @param callback User callback that renders PCM samples for output.
     */
    bool StartAudio(Callback callback);

    /**
     * @brief Enable or disable the headphone amplifier.
     * @param enabled True to un-shut down the amplifier.
     * @note The shutdown pin is pulled low on the board, so the amplifier stays off until
     *       the firmware explicitly enables it.
     */
    void SetAmplifierEnabled(bool enabled);

    /**
     * @brief Silences the output immediately: stops the DAC DMA and shuts the amplifier down.
     * @details Only plain register writes, so it is safe to call from a fault handler.
     */
    static void EmergencyStop();

    /**
     * @brief Get the effective PCM sample rate in Hz.
     * @return Effective output sample rate what the application sees.
     */
    float GetSampleRate() const;

    /**
     * @brief Get callback invocation rate in Hz.
     * @return Number of callback calls per second.
     */
    float GetCallbackRate() const;

private:
    /**
     * @brief GPIO pin on port B driving the amplifier's active-low shutdown input.
     */
    static constexpr uint16_t kAmplifierShutdownPin = GPIO_PIN_9;

    static constexpr size_t kDacSamplesPerDmaHalf = 64U;
    static constexpr size_t kProcessedSamplesPerDmaHalf = kDacSamplesPerDmaHalf / kCallbackDownscale;
    static constexpr size_t kDmaBufferWords = kDacSamplesPerDmaHalf * 2U;

    static_assert(kCallbackDownscale >= 1U, "Audio callback downscale must be >= 1");
    static_assert((kDacSamplesPerDmaHalf % kCallbackDownscale) == 0u,
                  "Audio callback downscale must divide DMA half-buffer size");

    /**
     * @brief Converts a stereo frame into a DHR12RD word feeding both DAC channels.
     */
    static uint32_t ToDualDacWord(Sample sample);

    bool InitPins();
    bool InitTimer();
    bool InitDac();
    bool InitDma();

    void FillBuffer(size_t half_index);
    void HandleHalfComplete();
    void HandleComplete();
    void HandleDmaError();
    void HandleDmaIrq();

    static void DmaHalfCompleteCallback(DMA_HandleTypeDef *hdma);
    static void DmaCompleteCallback(DMA_HandleTypeDef *hdma);
    static void DmaErrorCallback(DMA_HandleTypeDef *hdma);

    static AudioInterface *instance_;

    DAC_HandleTypeDef dac_{};
    TIM_HandleTypeDef sample_timer_{};
    DMA_HandleTypeDef dac_dma_{};
    Callback callback_ = nullptr;

    // Last frame the callback produced, the starting point for the next interpolation ramp.
    Sample previous_sample_{0, 0};

    // One word per DAC sample: channel 1 in the low half, channel 2 in the high half (DHR12RD).
    // Needs to be aligned to 4 bytes for DMA access.
    alignas(4) std::array<uint32_t, kDmaBufferWords> dac_dma_buffer_{};

    friend void ::DMA1_Channel1_IRQHandler(void);
};
} // namespace badger
