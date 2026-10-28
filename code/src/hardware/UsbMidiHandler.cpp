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


#include "hardware/UsbMidiHandler.hpp"

#include "py32f0xx_hal.h"
#include "tusb.h"

namespace badger
{

extern "C" void USB_IRQHandler(void)
{
    tud_int_handler(BOARD_TUD_RHPORT);
}

// TinyUSB needs a millisecond time base when built without an RTOS; the HAL tick is already running.
extern "C" uint32_t tusb_time_millis_api(void)
{
    return HAL_GetTick();
}

bool UsbMidiHandler::Init()
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_USB_CLK_ENABLE();

    // Lower priority than the MIDI UART (0) and the audio DMA (1) so USB never delays audio.
    HAL_NVIC_SetPriority(USB_IRQn, 3, 0);

    parser_.Reset();

    const tusb_rhport_init_t rhport_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };

    if (!tusb_init(BOARD_TUD_RHPORT, &rhport_init))
    {
        return false;
    }

    initialized_ = true;
    return true;
}

void UsbMidiHandler::SetPassThroughCallback(PassThroughCallback callback)
{
    passthrough_callback_ = callback;
}

void UsbMidiHandler::Update()
{
    if (!initialized_)
    {
        return;
    }

    tud_task();

    uint8_t buffer[kReadBufferSize];
    while (true)
    {
        const uint32_t count = tud_midi_stream_read(buffer, sizeof(buffer));
        if (count == 0)
        {
            break;
        }

        // USB IN -> TRS OUT passthrough. Forwarded as a raw stream so sysex survives.
        // Deliberately not echoed back to USB OUT, which would loop back into the host.
        if (passthrough_callback_ != nullptr)
        {
            passthrough_callback_(buffer, count);
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            parser_.ParseByte(buffer[i]);
        }
    }
}

void UsbMidiHandler::SendNoteOn(uint8_t note, uint8_t velocity)
{
    Midi::Message message = {Midi::Type::NOTE_ON, GetChannel(), note, velocity};
    Send(message);
}

void UsbMidiHandler::SendNoteOff(uint8_t note, uint8_t velocity)
{
    Midi::Message message = {Midi::Type::NOTE_OFF, GetChannel(), note, velocity};
    Send(message);
}

void UsbMidiHandler::SendRealTime(Midi::Type type)
{
    Midi::Message message = {type, 0, 0, 0};
    Send(message);
}

void UsbMidiHandler::Send(Midi::Message message)
{
    uint8_t bytes[MidiParser::kMaxMessageBytes] = {};
    const size_t total = MidiParser::Encode(message, bytes);

    SendRaw(bytes, total);
}

void UsbMidiHandler::SendRaw(const uint8_t *data, size_t length)
{
    if (data == nullptr || length == 0 || !IsConnected())
    {
        return;
    }

    // Bytes that don't fit the TX FIFO are dropped, same as the TRS ring buffer does.
    tud_midi_stream_write(kCableNumber, data, static_cast<uint32_t>(length));
}

void UsbMidiHandler::SetRealTimeMessageCallback(RealTimeMessageCallback callback)
{
    parser_.SetRealTimeMessageCallback(callback);
}

void UsbMidiHandler::SetChannelMessageCallback(ChannelMessageCallback callback)
{
    parser_.SetChannelMessageCallback(callback);
}

void UsbMidiHandler::SetChannel(uint8_t channel)
{
    parser_.SetChannel(channel);
}

uint8_t UsbMidiHandler::GetChannel() const
{
    return parser_.GetChannel();
}

bool UsbMidiHandler::IsConnected() const
{
    return initialized_ && tud_midi_mounted();
}

} // namespace badger
