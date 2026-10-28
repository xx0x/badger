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

#include "EightBitSamplePlayer.hpp"

#include <cmath>

namespace badger
{

void EightBitSamplePlayer::Init(float sample_rate, float sample_rate_of_sample, uint8_t bit_depth)
{
    sample_rate_ = sample_rate;
    sample_rate_of_sample_ = sample_rate_of_sample;
    bit_depth_ = bit_depth;
    if (bit_depth_ == 0 || bit_depth_ > 8)
    {
        bit_depth_ = 8;
    }

    state_ = State::STOPPED;
    playback_index_ = 0.0f;
    sample_count_ = 0;
    output_ = Q15_ZERO;
    speed_ = 1.0f;
    UpdateIncrement();
}

void EightBitSamplePlayer::Load(std::span<const uint8_t> sample_data)
{
    const size_t inferred_sample_count = bit_depth_ == 0 ? 0 : ((sample_data.size() * 8u) / bit_depth_);
    Load(sample_data, inferred_sample_count);
}

void EightBitSamplePlayer::Load(std::span<const uint8_t> sample_data, size_t sample_count)
{
    sample_data_ = sample_data;
    sample_count_ = sample_count;
    state_ = State::STOPPED;
    playback_index_ = 0.0f;
    output_ = Q15_ZERO;
}

void EightBitSamplePlayer::Trigger()
{
    playback_index_ = 0.0f;
    output_ = Q15_ZERO;

    if (sample_data_.empty())
    {
        state_ = State::STOPPED;
        return;
    }

    state_ = State::PLAYING;
}

void EightBitSamplePlayer::SetSpeed(float speed)
{
    speed = std::max(speed, 0.01f); // Prevent extremely slow speeds that could cause issues
    speed_ = speed;
    UpdateIncrement();
}

q15_t EightBitSamplePlayer::Process()
{
    if (state_ != State::PLAYING || sample_data_.empty() || sample_count_ == 0)
    {
        output_ = Q15_ZERO;
        return output_;
    }

    const size_t sample_index = static_cast<size_t>(playback_index_);
    if (sample_index >= sample_count_)
    {
        state_ = State::STOPPED;
        output_ = Q15_ZERO;
        return output_;
    }

    output_ = U8ToQ15(ReadPackedSample(sample_index));
    playback_index_ += playback_increment_;
    return output_;
}

q15_t EightBitSamplePlayer::GetOutput()
{
    return output_;
}

q15_t EightBitSamplePlayer::U8ToQ15(uint8_t sample)
{
    return static_cast<q15_t>((static_cast<int32_t>(sample) - 128) << 8);
}

uint8_t EightBitSamplePlayer::ReadPackedSample(size_t sample_index) const
{
    if (bit_depth_ == 8)
    {
        return sample_data_[sample_index];
    }

    const size_t base_bit = sample_index * bit_depth_;
    uint8_t quantized = 0;
    for (uint8_t bit = 0; bit < bit_depth_; ++bit)
    {
        const size_t src_bit = base_bit + bit;
        const size_t src_byte_index = src_bit / 8u;
        if (src_byte_index >= sample_data_.size())
        {
            break;
        }

        const uint8_t src_bit_in_byte = static_cast<uint8_t>(7u - (src_bit % 8u));
        const uint8_t bit_value = static_cast<uint8_t>((sample_data_[src_byte_index] >> src_bit_in_byte) & 0x01u);
        quantized = static_cast<uint8_t>((quantized << 1u) | bit_value);
    }

    return static_cast<uint8_t>(quantized << (8u - bit_depth_));
}

void EightBitSamplePlayer::UpdateIncrement()
{
    if (sample_rate_ <= 0.0f)
    {
        playback_increment_ = 0.0f;
        return;
    }

    playback_increment_ = (sample_rate_of_sample_ / sample_rate_) * speed_;
}

} // namespace badger
