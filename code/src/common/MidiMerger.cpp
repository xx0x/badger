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

#include "common/MidiMerger.hpp"

#include <algorithm>
#include <utility>

#include "common/MidiParser.hpp"

namespace badger
{

void MidiMerger::SetSink(Sink sink)
{
    sink_ = std::move(sink);
}

void MidiMerger::Reset()
{
    for (auto &assembler : assemblers_)
    {
        assembler = Assembler{};
    }
    sysex_owner_ = kNoInput;
    held_length_ = 0;
}

void MidiMerger::Write(Input input, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        Write(input, data[i]);
    }
}

void MidiMerger::Write(Input input, uint8_t byte)
{
    if (input == kNoInput)
    {
        return;
    }

    // Real-time bytes are allowed anywhere, even in the middle of other messages
    if (byte >= kFirstRealTime)
    {
        Emit(&byte, 1);
        return;
    }

    Assembler &assembler = assemblers_[input];
    if ((byte & 0x80U) != 0U)
    {
        HandleStatus(input, assembler, byte);
    }
    else
    {
        HandleData(input, assembler, byte);
    }
}

void MidiMerger::HandleStatus(Input input, Assembler &assembler, uint8_t status)
{
    // Any status byte ends a SysEx in progress, an explicit end of SysEx just does so cleanly
    if (assembler.in_sysex)
    {
        EndSysex(assembler);
        if (status == kSysexEnd)
        {
            return;
        }
    }
    else if (status == kSysexEnd)
    {
        // Stray end of SysEx
        return;
    }

    assembler.status = 0;
    assembler.received_data_bytes = 0;

    if (status == kSysexStart)
    {
        assembler.running_status = 0;
        assembler.in_sysex = true;

        // Two SysEx messages can't be merged, a second one arriving meanwhile is dropped
        assembler.dropping_sysex = sysex_owner_ != kNoInput && sysex_owner_ != input;
        if (!assembler.dropping_sysex)
        {
            sysex_owner_ = input;
            PassSysex(assembler, kSysexStart);
        }
        return;
    }

    assembler.status = status;
    assembler.expected_data_bytes = MidiParser::GetDataLength(status);

    // System common messages cancel running status
    assembler.running_status = status < kSysexStart ? status : 0;

    if (assembler.expected_data_bytes == 0)
    {
        CompleteMessage(input, assembler);
    }
}

void MidiMerger::HandleData(Input input, Assembler &assembler, uint8_t byte)
{
    if (assembler.in_sysex)
    {
        if (!assembler.dropping_sysex)
        {
            PassSysex(assembler, byte);
        }
        return;
    }

    if (assembler.status == 0)
    {
        // Running status, or a stray data byte without any status to belong to
        if (assembler.running_status == 0)
        {
            return;
        }
        assembler.status = assembler.running_status;
        assembler.expected_data_bytes = MidiParser::GetDataLength(assembler.status);
        assembler.received_data_bytes = 0;
    }

    if (assembler.received_data_bytes < assembler.data.size())
    {
        assembler.data[assembler.received_data_bytes] = byte;
    }

    if (++assembler.received_data_bytes >= assembler.expected_data_bytes)
    {
        CompleteMessage(input, assembler);
    }
}

void MidiMerger::CompleteMessage(Input input, Assembler &assembler)
{
    const std::array<uint8_t, 3> message = {assembler.status, assembler.data[0], assembler.data[1]};
    const size_t length = 1U + assembler.expected_data_bytes;

    assembler.status = 0;
    assembler.received_data_bytes = 0;

    // Another input's SysEx is going out, this has to wait until it ends
    if (sysex_owner_ != kNoInput && sysex_owner_ != input)
    {
        Hold(message.data(), length);
        return;
    }

    Emit(message.data(), length);
}

void MidiMerger::PassSysex(Assembler &assembler, uint8_t byte)
{
    sysex_last_activity_ms_ = now_ms_;

    if (!Emit(&byte, 1))
    {
        // The output is full, so the SysEx is broken anyway: drop the rest of it and let the others through
        // (the next status byte ends it on the receiving side)
        assembler.dropping_sysex = true;
        ReleaseSysex();
    }
}

void MidiMerger::EndSysex(Assembler &assembler)
{
    if (!assembler.dropping_sysex)
    {
        const uint8_t end = kSysexEnd;
        Emit(&end, 1);
        ReleaseSysex();
    }

    assembler.in_sysex = false;
    assembler.dropping_sysex = false;
}

void MidiMerger::ReleaseSysex()
{
    sysex_owner_ = kNoInput;
    FlushHeld();
}

void MidiMerger::Hold(const uint8_t *data, size_t length)
{
    // No room left, the message is dropped
    if (held_length_ + length > held_.size())
    {
        return;
    }

    std::copy(data, data + length, held_.begin() + static_cast<std::ptrdiff_t>(held_length_));
    held_length_ += length;
}

void MidiMerger::FlushHeld()
{
    // Held messages always carry their status byte, which tells where each one starts and how long it is
    size_t i = 0;
    while (i < held_length_)
    {
        const size_t length = 1U + MidiParser::GetDataLength(held_[i]);
        Emit(&held_[i], std::min(length, held_length_ - i));
        i += length;
    }
    held_length_ = 0;
}

void MidiMerger::Process(uint32_t now_ms)
{
    now_ms_ = now_ms;

    if (sysex_owner_ != kNoInput && now_ms_ - sysex_last_activity_ms_ > kSysexTimeoutMs)
    {
        // Abandoned halfway: close it on the output and discard whatever of it still arrives
        const uint8_t end = kSysexEnd;
        Emit(&end, 1);
        assemblers_[sysex_owner_].dropping_sysex = true;
        ReleaseSysex();
    }
}

bool MidiMerger::Emit(const uint8_t *data, size_t length)
{
    return sink_ ? sink_(data, length) : false;
}

} // namespace badger
