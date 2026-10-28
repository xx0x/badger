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

#include "common/Midi.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace badger
{

/**
 * @brief Parses a raw MIDI byte stream into messages and encodes messages back into bytes.
 * @details Transport agnostic: the same parser backs the TRS UART port (`MidiHandler`) and
 *          the USB MIDI port (`UsbMidiHandler`), so both behave identically for notes,
 *          control changes, running status, and realtime messages.
 * @author Vaclav Mach
 * @date 2026-08-21
 */
class MidiParser
{
public:
    /**
     * @brief Callback type for parsed MIDI messages.
     */
    using MessageCallback = std::function<void(const Midi::Message &message)>;

    /**
     * @brief Maximum number of bytes a single (non-sysex) MIDI message encodes to.
     */
    static constexpr size_t kMaxMessageBytes = 3;

    /**
     * @brief Clears the parser state machine (running status, sysex, pending data bytes).
     */
    void Reset();

    /**
     * @brief Feeds one received byte into the parser, invoking callbacks on complete messages.
     * @param byte Incoming MIDI byte.
     */
    void ParseByte(uint8_t byte);

    /**
     * @brief Register a callback for incoming MIDI realtime messages.
     * @param callback Callback function.
     */
    void SetRealTimeMessageCallback(MessageCallback callback);

    /**
     * @brief Register a callback for incoming MIDI channel messages.
     * @param callback Callback function.
     */
    void SetChannelMessageCallback(MessageCallback callback);

    /**
     * @brief Register a callback receiving every parsed message, unfiltered by channel.
     * @details Used for port-to-port passthrough: it also sees realtime and system common
     *          messages, which the channel callback never gets. Sysex is not reported.
     * @param callback Callback function.
     */
    void SetForwardCallback(MessageCallback callback);

    /**
     * @brief Set the active MIDI channel.
     * @param channel Zero-indexed MIDI channel (0-15) or `Midi::OMNI_CHANNEL` to listen to all channels.
     */
    void SetChannel(uint8_t channel);

    /**
     * @brief Get the active MIDI channel (0-15) or `Midi::OMNI_CHANNEL` for omni mode.
     * @return Current channel setting.
     */
    uint8_t GetChannel() const;

    /**
     * @brief Encodes a MIDI message into its wire bytes.
     * @param message Message to encode.
     * @param bytes Destination buffer, must hold at least `kMaxMessageBytes`.
     * @return Number of bytes written.
     * @note Channel messages fall back to channel 0 when the message carries `Midi::OMNI_CHANNEL`.
     */
    static size_t Encode(const Midi::Message &message, uint8_t (&bytes)[kMaxMessageBytes]);

    /**
     * @brief Returns how many data bytes follow the given status byte.
     * @param status MIDI status byte.
     */
    static uint8_t GetDataLength(uint8_t status);

private:
    static Midi::Message BuildMessage(uint8_t status, uint8_t data1, uint8_t data2);
    void HandleRealtimeStatus(uint8_t status);
    void CompleteMessage();

    uint8_t channel_ = Midi::OMNI_CHANNEL; // MIDI channel (zero indexed 0-15) or 255 (`Midi::OMNI_CHANNEL`)
    bool in_sysex_ = false;

    uint8_t running_status_ = 0;
    uint8_t current_status_ = 0;
    uint8_t expected_data_bytes_ = 0;
    uint8_t received_data_bytes_ = 0;
    uint8_t data_bytes_[2] = {0, 0};

    MessageCallback realtime_message_callback_;
    MessageCallback channel_message_callback_;
    MessageCallback forward_callback_;
};

}
