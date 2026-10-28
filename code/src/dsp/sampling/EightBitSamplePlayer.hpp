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
#include "dsp/math/qmath.hpp"

namespace badger
{

/**
 * @brief Lo-fi 8-bit sample playback engine. Use with 8-bit unsigned PCM samples.
 * @author Vaclav Mach
 * @date 2026-06-04
 */
class EightBitSamplePlayer
{
public:
    /**
     * @brief Playback state of the sample player.
     */
    enum class State
    {
        STOPPED,
        PLAYING
    };

    /**
     * @brief Initializes the sample player. Must be called before using any other methods.
     * @param sample_rate The sample rate of the audio output (e.g. 44100.0f).
     * @param sample_rate_of_sample The original sample rate of the loaded sample (e.g. 16000.0f). This is used to calculate the correct playback speed.
     * @param bit_depth Bit depth of the sample stream in range 1..8. For packed data use the same value as used during compression.
     */
    void Init(float sample_rate, float sample_rate_of_sample, uint8_t bit_depth = 8);

    /**
     * @brief Loads an 8-bit unsigned PCM sample into the player.
     * @param sample_data A span containing the raw 8-bit unsigned PCM sample data.
     */
    void Load(std::span<const uint8_t> sample_data);

    /**
     * @brief Loads packed sample data with explicit sample count.
     * @param sample_data A span containing packed unsigned PCM sample data.
     * @param sample_count Number of samples represented by sample_data (doesn't need to match sample_data.size() if the data is packed for a bit depth less than 8).
     */
    void Load(std::span<const uint8_t> sample_data, size_t sample_count);

    /**
     * @brief Resets the playback position to the beginning of the sample and starts playback.
     */
    void Trigger();

    /**
     * @brief Sets the playback speed. 1.0f is normal speed, 2.0f is one octave up, 0.5f is one octave down, etc.
     * @param speed Playback speed multiplier.
     */
    void SetSpeed(float speed);

    /**
     * @brief Returns the current sample and advances the playback position. Should be called at the sample rate of the audio output.
     * @return The current sample as a q15_t value. Returns 0 if the sample is not playing.
     */
    q15_t Process();

    /**
     * @brief Returns the cached sample.
     */
    q15_t GetOutput();

    /**
     * @brief Returns the current state of the sample player (STOPPED or PLAYING).
     */
    State GetState() const { return state_; }

    /**
     * @brief Struct representing a compressed sample. Use the static Compress() method to create an instance of this struct at compile time.
     */
    template <size_t Bytes, size_t Samples, uint32_t SampleRate, uint8_t BitDepth>
    struct PackedSample
    {
        /**
         * @brief Number of samples in the compressed sample (doesn't match size of data if the sample is packed for a bit depth less than 8).
         */
        static constexpr size_t sample_count = Samples;

        /**
         * @brief Sample rate of the compressed sample.
         */
        static constexpr float sample_rate = static_cast<float>(SampleRate);

        /**
         * @brief Bit depth of the compressed sample (1 to 8).
         */
        static constexpr uint8_t bit_depth = BitDepth;

        /**
         * @brief Actual packed sample data.
         */
        std::array<uint8_t, Bytes> data{};
    };

    /**
     * @brief Compresses an 8-bit unsigned PCM sample into a packed format with the specified bit depth and target sample rate. This method is consteval and must be used at compile time.
     * @tparam TargetSampleRate The desired sample rate for the output sample.
     * @tparam TargetBitDepth The desired bit depth for the output sample (1 to 8).
     * @tparam SourceSampleRate The sample rate of the input sample (default is 8000).
     * @tparam N The number of bytes in the input sample data.
     * @param sample_data The input sample data as a std::array of 8-bit unsigned PCM samples.
     * @return A PackedSample struct containing the packed sample data, sample count, sample rate, and bit depth.
     * @note The input sample data must be in 8-bit unsigned PCM format. The output sample will be packed according to the specified bit depth, and the sample rate will be adjusted to the target sample rate. The output sample count will be calculated based on the input sample count and the ratio of the source and target sample rates.
     * @note This method is intended for use at compile time and will produce a compile-time error if used at runtime. Use the Load() method to load sample data at runtime.
     */
    template <uint32_t TargetSampleRate, uint8_t TargetBitDepth, uint32_t SourceSampleRate = 8000, size_t N>
    static consteval auto Compress(const std::array<uint8_t, N> &sample_data)
    {
        static_assert(TargetSampleRate > 0, "TargetSampleRate must be greater than 0");
        static_assert(SourceSampleRate > 0, "SourceSampleRate must be greater than 0");
        static_assert(TargetBitDepth >= 1 && TargetBitDepth <= 8, "TargetBitDepth must be in range 1..8");

        constexpr size_t kOutputSampleCount = (N * TargetSampleRate + SourceSampleRate - 1) / SourceSampleRate;
        constexpr size_t kOutputBitCount = kOutputSampleCount * TargetBitDepth;
        constexpr size_t kOutputByteCount = (kOutputBitCount + 7) / 8;

        PackedSample<kOutputByteCount, kOutputSampleCount, TargetSampleRate, TargetBitDepth> result{};

        for (size_t out_index = 0; out_index < kOutputSampleCount; ++out_index)
        {
            const size_t src_index = (out_index * SourceSampleRate) / TargetSampleRate;
            const uint8_t sample = sample_data[src_index < N ? src_index : (N - 1)];
            const uint8_t quantized = static_cast<uint8_t>(sample >> (8u - TargetBitDepth));

            const size_t base_bit = out_index * TargetBitDepth;
            for (uint8_t bit = 0; bit < TargetBitDepth; ++bit)
            {
                const bool bit_set = ((quantized >> (TargetBitDepth - 1u - bit)) & 0x01u) != 0u;
                if (!bit_set)
                {
                    continue;
                }

                const size_t dst_bit = base_bit + bit;
                const size_t dst_byte_index = dst_bit / 8u;
                const uint8_t dst_bit_in_byte = static_cast<uint8_t>(7u - (dst_bit % 8u));
                result.data[dst_byte_index] = static_cast<uint8_t>(result.data[dst_byte_index] | static_cast<uint8_t>(1u << dst_bit_in_byte));
            }
        }

        return result;
    }

private:
    void UpdateIncrement();
    static q15_t U8ToQ15(uint8_t sample);

    uint8_t ReadPackedSample(size_t sample_index) const;

    std::span<const uint8_t> sample_data_{};
    size_t sample_count_ = 0;
    State state_ = State::STOPPED;

    uint8_t bit_depth_ = 8;
    float sample_rate_ = 1.0f;
    float sample_rate_of_sample_ = 1.0f;
    float speed_ = 0.0f;
    float playback_index_ = 0.0f;
    float playback_increment_ = 0.0f;
    q15_t output_ = Q15_ZERO;
};

}