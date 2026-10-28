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
#include "common/MidiMerger.hpp"
#include "common/MidiParser.hpp"
#include "py32f0xx_hal.h"

#include <cstddef>

namespace badger
{

/**
 * @brief Handles TRS MIDI UART I/O, message parsing, and optional passthrough.
 * @author Vaclav Mach
 * @date 2026-06-05
 */
class MidiHandler
{
public:
    /**
     * @brief Callback type for MIDI realtime messages (=clock).
     */
    using RealTimeMessageCallback = MidiParser::MessageCallback;

    /**
     * @brief Callback type for MIDI channel notes and control changes.
     */
    using ChannelMessageCallback = MidiParser::MessageCallback;

    /**
     * @brief Callback type for forwarding received messages to another MIDI port.
     */
    using PassThroughCallback = void (*)(const Midi::Message &message);

    /**
     * @brief Initialize UART and MIDI handler runtime state.
     * @return True if initialization was successful, false otherwise.
     */
    bool Init();

    /**
     * @brief Handles reading the data from the UART
     */
    void Update();

    /**
     * @brief Enable or disable TRS IN -> TRS OUT passthrough.
     * @param enable True to enable passthrough.
     * @details Real-time bytes are passed on straight from the RX interrupt, everything else is merged
     *          with the other outgoing traffic message by message (see MidiMerger), so the Badger's own
     *          messages never land in the middle of a passed through one.
     * @note Gates both the TRS OUT passthrough and the forward callback (USB OUT).
     */
    void EnablePassThrough(bool enable);

    /**
     * @brief Registers a sink for received messages, used for TRS IN -> USB OUT passthrough.
     * @details Parsed messages rather than raw bytes: the USB MIDI packetizer needs an explicit
     *          status byte per message, so it cannot take running status or a realtime byte
     *          interleaved mid-message. Parsing normalizes both. Sysex is not forwarded to USB
     *          (the TRS OUT passthrough still carries it).
     * @param callback Callback function, or nullptr to disable forwarding.
     */
    void SetPassThroughCallback(PassThroughCallback callback);

    /**
     * @brief Send a MIDI Note On message on the configured channel.
     * @param note MIDI note number.
     * @param velocity Note velocity.
     */
    void SendNoteOn(uint8_t note, uint8_t velocity);

    /**
     * @brief Send a MIDI Note Off message on the configured channel.
     * @param note MIDI note number.
     * @param velocity Release velocity.
     */
    void SendNoteOff(uint8_t note, uint8_t velocity);

    /**
     * @brief Send a MIDI realtime message.
     * @param type MIDI realtime message type.
     */
    void SendRealTime(Midi::Type type);

    /**
     * @brief Send a complete MIDI message.
     * @param message Message to transmit.
     */
    void Send(Midi::Message message);

    /**
     * @brief Forwards a raw MIDI byte stream received over USB, for USB IN -> TRS OUT passthrough.
     * @details The stream may contain partial messages, running status and SysEx, it is merged with
     *          the other outgoing traffic message by message (see MidiMerger).
     * @param data Received bytes.
     * @param length Number of bytes.
     */
    void ForwardFromUsb(const uint8_t *data, size_t length);

    /**
     * @brief Register a callback for incoming MIDI realtime messages.
     * @param callback Callback function.
     */
    void SetRealTimeMessageCallback(RealTimeMessageCallback callback);

    /**
     * @brief Register a callback for incoming MIDI channel messages.
     * @param callback Callback function.
     */
    void SetChannelMessageCallback(ChannelMessageCallback callback);

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
     * @brief UART interrupt service routine entry point.
     */
    static void IrqHandler();

private:
    void ForwardMessage(const Midi::Message &message);

    /**
     * @brief Appends bytes to the TX ring all or nothing, and starts the transmission.
     * @return False if they didn't fit, nothing is appended then.
     */
    bool TxAppend(const uint8_t *data, size_t length);

    MidiParser parser_;
    MidiMerger merger_;
    PassThroughCallback passthrough_callback_ = nullptr;

    static constexpr size_t kRxRingBufferSize = 128;
    volatile uint8_t rx_head_ = 0;
    volatile uint8_t rx_tail_ = 0;
    uint8_t rx_buffer_[kRxRingBufferSize] = {};

    static constexpr size_t kTxRingBufferSize = 128;
    volatile uint8_t tx_head_ = 0;
    volatile uint8_t tx_tail_ = 0;
    uint8_t tx_buffer_[kTxRingBufferSize] = {};

    static MidiHandler *instance_;

    UART_HandleTypeDef uart_ = {};
    bool passthrough_enabled_ = false;
    static constexpr uint32_t kMidiBaudRate = 31250;
};

}
