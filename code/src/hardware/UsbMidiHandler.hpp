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
#include "common/MidiParser.hpp"

#include <cstddef>
#include <cstdint>

namespace badger
{

/**
 * @brief Handles USB MIDI device I/O, message parsing, and passthrough to the TRS port.
 * @details Mirrors `MidiHandler` (they share `MidiParser`), so notes, control changes and
 *          realtime messages behave identically on both ports. There is deliberately no
 *          USB IN -> USB OUT passthrough, which would loop straight back into the host.
 * @author Vaclav Mach
 * @date 2026-08-21
 */
class UsbMidiHandler
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
     * @brief Callback type for forwarding received raw bytes to another MIDI port.
     */
    using PassThroughCallback = void (*)(const uint8_t *data, size_t length);

    /**
     * @brief Initialize the USB peripheral and the TinyUSB device stack.
     * @return True if initialization was successful, false otherwise.
     */
    bool Init();

    /**
     * @brief Runs the USB device task and parses whatever the host has sent.
     */
    void Update();

    /**
     * @brief Registers a sink for raw received bytes, used for USB IN -> TRS OUT passthrough.
     * @details Raw bytes rather than parsed messages: every USB MIDI event packet already carries
     *          its own status byte, so the byte stream is well formed and sysex passes through.
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
     * @brief Queue raw MIDI bytes for transmission, used for TRS IN -> USB OUT passthrough.
     * @param data Bytes to transmit.
     * @param length Number of bytes.
     */
    void SendRaw(const uint8_t *data, size_t length);

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
     * @brief True when the host has configured the device and MIDI traffic can flow.
     */
    bool IsConnected() const;

private:
    /**
     * @brief USB MIDI virtual cable used for all traffic. The Badger exposes a single port.
     */
    static constexpr uint8_t kCableNumber = 0;

    /**
     * @brief Read chunk size, matching the full-speed bulk endpoint packet size.
     */
    static constexpr size_t kReadBufferSize = 64;

    MidiParser parser_;
    PassThroughCallback passthrough_callback_ = nullptr;
    bool initialized_ = false;
};

}
