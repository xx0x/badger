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


#include "common/MidiParser.hpp"

namespace badger
{

void MidiParser::Reset()
{
    in_sysex_ = false;
    running_status_ = 0;
    current_status_ = 0;
    expected_data_bytes_ = 0;
    received_data_bytes_ = 0;
}

void MidiParser::SetRealTimeMessageCallback(MessageCallback callback)
{
    realtime_message_callback_ = callback;
}

void MidiParser::SetChannelMessageCallback(MessageCallback callback)
{
    channel_message_callback_ = callback;
}

void MidiParser::SetForwardCallback(MessageCallback callback)
{
    forward_callback_ = callback;
}

void MidiParser::SetChannel(uint8_t channel)
{
    if (channel <= 15 || channel == Midi::OMNI_CHANNEL)
    {
        channel_ = channel;
    }
}

uint8_t MidiParser::GetChannel() const
{
    return channel_;
}

size_t MidiParser::Encode(const Midi::Message &message, uint8_t (&bytes)[kMaxMessageBytes])
{
    const uint8_t type = static_cast<uint8_t>(message.type);
    uint8_t status = type;

    // If omni channel is selected for receiving, send on channel 0 (MIDI channel 1).
    uint8_t send_channel = message.channel;
    if (send_channel > 15)
    {
        send_channel = 0;
    }

    if ((type & 0xF0U) != 0xF0U)
    {
        status = static_cast<uint8_t>(type | (send_channel & 0x0FU));
    }

    bytes[0] = status;
    bytes[1] = message.data1;
    bytes[2] = message.data2;

    return static_cast<size_t>(1U + GetDataLength(status));
}

uint8_t MidiParser::GetDataLength(uint8_t status)
{
    if (status < 0x80U)
    {
        return 0;
    }

    if (status < 0xF0U)
    {
        const uint8_t type_nibble = static_cast<uint8_t>(status & 0xF0U);
        if (type_nibble == static_cast<uint8_t>(Midi::Type::PROGRAM_CHANGE) ||
            type_nibble == static_cast<uint8_t>(Midi::Type::CHANNEL_AFTERTOUCH))
        {
            return 1;
        }
        return 2;
    }

    switch (status)
    {
    case static_cast<uint8_t>(Midi::Type::MTC_QUARTER_FRAME):
    case static_cast<uint8_t>(Midi::Type::SONG_SELECT):
        return 1;
    case static_cast<uint8_t>(Midi::Type::SONG_POSITION_POINTER):
        return 2;
    default:
        return 0;
    }
}

Midi::Message MidiParser::BuildMessage(uint8_t status, uint8_t data1, uint8_t data2)
{
    Midi::Message message = {};
    if ((status & 0xF0U) != 0xF0U)
    {
        message.type = static_cast<Midi::Type>(status & 0xF0U);
        message.channel = static_cast<uint8_t>(status & 0x0FU);
    }
    else
    {
        message.type = static_cast<Midi::Type>(status);
        message.channel = 0;
    }

    message.data1 = data1;
    message.data2 = data2;
    return message;
}

void MidiParser::HandleRealtimeStatus(uint8_t status)
{
    const Midi::Message message = BuildMessage(status, 0, 0);

    if (realtime_message_callback_)
    {
        realtime_message_callback_(message);
    }

    if (forward_callback_)
    {
        forward_callback_(message);
    }
}

void MidiParser::CompleteMessage()
{
    const uint8_t data1 = expected_data_bytes_ >= 1 ? data_bytes_[0] : 0;
    const uint8_t data2 = expected_data_bytes_ >= 2 ? data_bytes_[1] : 0;
    const Midi::Message message = BuildMessage(current_status_, data1, data2);

    if (forward_callback_)
    {
        forward_callback_(message);
    }

    if (current_status_ < 0xF0U && channel_message_callback_)
    {
        uint8_t channel = message.channel;
        if (channel_ == Midi::OMNI_CHANNEL || channel == channel_)
        {
            channel_message_callback_(message);
        }
    }
}

void MidiParser::ParseByte(uint8_t byte)
{
    if (byte >= static_cast<uint8_t>(Midi::Type::CLOCK))
    {
        HandleRealtimeStatus(byte);
        return;
    }

    if (byte & 0x80U)
    {
        if (byte == static_cast<uint8_t>(Midi::Type::SYSTEM_EXCLUSIVE))
        {
            in_sysex_ = true;
            running_status_ = 0;
            current_status_ = 0;
            expected_data_bytes_ = 0;
            received_data_bytes_ = 0;
            return;
        }

        if (byte == static_cast<uint8_t>(Midi::Type::END_OF_EXCLUSIVE))
        {
            in_sysex_ = false;
            current_status_ = 0;
            expected_data_bytes_ = 0;
            received_data_bytes_ = 0;
            return;
        }

        in_sysex_ = false;
        current_status_ = byte;
        expected_data_bytes_ = GetDataLength(byte);
        received_data_bytes_ = 0;

        if (byte < 0xF0U)
        {
            running_status_ = byte;
        }
        else
        {
            running_status_ = 0;
        }

        if (expected_data_bytes_ == 0)
        {
            CompleteMessage();
            if (current_status_ >= 0xF0U)
            {
                current_status_ = 0;
            }
        }
        return;
    }

    if (in_sysex_)
    {
        return;
    }

    if (current_status_ == 0)
    {
        if (running_status_ == 0)
        {
            return;
        }

        current_status_ = running_status_;
        expected_data_bytes_ = GetDataLength(current_status_);
        received_data_bytes_ = 0;
    }

    if (expected_data_bytes_ == 0)
    {
        return;
    }

    if (received_data_bytes_ < 2)
    {
        data_bytes_[received_data_bytes_] = byte;
    }
    received_data_bytes_++;

    if (received_data_bytes_ >= expected_data_bytes_)
    {
        CompleteMessage();
        received_data_bytes_ = 0;

        if (current_status_ >= 0xF0U)
        {
            current_status_ = 0;
            expected_data_bytes_ = 0;
        }
    }
}

} // namespace badger
