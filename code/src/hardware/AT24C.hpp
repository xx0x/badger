/*
MIT License

Copyright (c) 2024 Vaclav Mach (Bastl Instruments)

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

#include <cstddef>
#include <cstdint>

#include "py32f0xx_hal.h"

namespace badger
{

/**
 * @class AT24C
 * @ingroup peripherals
 * @brief Driver for EEPROM memories (24XX02, 24XX32, ...)
 * @details
 * The Badger carries a single 24C02 on I2C2 (SCL on PB13, SDA on PB14, both with external
 * pull-ups), so this driver owns that bus outright - Init() brings up the pins and the
 * peripheral before doing anything else.
 * @note Don't access directly, use Badger::eeprom instead.
 * @author Vaclav Mach (Bastl Instruments)
 * @date 2024-04-22
 */
class AT24C
{
public:
    /**
     * Raw constructor, you can use this or predefined static functions.
     *
     * @param size Size of the memory in bytes
     * @param address_size Width of the memory's word address in bytes (1 or 2)
     * @param page_size Size of the memory page in bytes
     */
    constexpr AT24C(size_t size, size_t address_size, size_t page_size)
        : size_(size), address_size_(address_size), page_size_(page_size)
    {
    }

    /** AT24C02 geometry (2 kBits => 256 Bytes, 8 byte pages, 1 byte word address) */
    static constexpr size_t kAT24C02Size = 256;
    static constexpr size_t kAT24C02PageSize = 8;

    /** AT24C32 geometry (32 kBits => 4 kBytes, 32 byte pages, 2 byte word address) */
    static constexpr size_t kAT24C32Size = 4096;
    static constexpr size_t kAT24C32PageSize = 32;

    /**
     * AT24C02 (2 kBits => 256 Bytes)
     * @return AT24C object
     */
    static constexpr AT24C AT24C02()
    {
        return AT24C(kAT24C02Size, 1, kAT24C02PageSize);
    }

    /**
     * AT24C32 (32 kBits => 4 kBytes)
     * @return AT24C object
     */
    static constexpr AT24C AT24C32()
    {
        return AT24C(kAT24C32Size, 2, kAT24C32PageSize);
    }

    /**
     * Sets up the I2C peripheral and its pins,
     * doesn't talk to the memory yet.
     *
     * @param i2c_address 7-bit device address of the memory
     * @return True if the I2C peripheral came up, false otherwise
     */
    bool Init(uint8_t i2c_address = kDefaultI2cAddress);

    /**
     * Checks whether the memory acknowledges its address on the bus
     */
    bool IsConnected();

    /**
     * Writes an array of bytes, blocking until it is done
     * If the writing fails (eg. chip not available), returns false
     * @note Pages that already hold the requested bytes are skipped, which keeps small
     *       updates quick and spares the memory a needless write cycle.
     */
    bool Write(uint16_t address, const uint8_t *data, size_t len);

    /**
     * Progress of a write started with StartWrite()
     */
    enum class WriteStatus
    {
        BUSY,   ///< Not finished yet, call ProcessWrite() again later
        DONE,   ///< All bytes are stored (or no write was in progress)
        FAILED, ///< The memory didn't respond, the write was abandoned
    };

    /**
     * Starts writing an array of bytes without blocking, ProcessWrite() then carries it out
     * @note `data` has to stay valid and unchanged until ProcessWrite() stops returning BUSY.
     *       Starting a new write abandons one still in progress.
     * @return False if the range doesn't fit into the memory
     */
    bool StartWrite(uint16_t address, const uint8_t *data, size_t len);

    /**
     * Advances a write started with StartWrite() by one small step, never waiting for the memory:
     * one page is compared and written if it differs, or the end of its write cycle is checked for once.
     * A step takes a fraction of a millisecond, so this can be called from the UI loop.
     */
    WriteStatus ProcessWrite();

    /**
     * Writes a single byte
     * If the writing fails (eg. chip not available), returns false
     */
    bool WriteByte(uint16_t address, uint8_t data);

    /**
     * Reads an array of bytes
     * If the reading fails (eg. chip not available), returns false
     */
    bool Read(uint16_t address, uint8_t *data, size_t len);

    /**
     * Reads a single byte
     * If the reading fails (eg. chip not available), returns false
     */
    bool ReadByte(uint16_t address, uint8_t *data);

    /**
     * Size of the memory in bytes
     */
    constexpr size_t Size() const
    {
        return size_;
    }

    /**
     * Size of one memory page in bytes, the largest chunk a single write cycle can take
     */
    constexpr size_t PageSize() const
    {
        return page_size_;
    }

    /**
     * Default memory address
     */
    static constexpr uint8_t kDefaultI2cAddress = 0x50;

    /**
     * Bus clock, comfortably within the memory's 400 kHz rating
     */
    static constexpr uint32_t kI2cClockSpeed = 400000;

    /**
     * Simple timeout for writing. A page transfer is a fraction of a millisecond, so this is
     * pure headroom - it only ever bites when the bus is jammed, and then it bounds how long
     * a dead memory can hold up the caller.
     */
    static constexpr uint32_t kWriteTimeout = 50;

    /**
     * Simple timeout for reading, generous next to the few milliseconds a full read takes.
     */
    static constexpr uint32_t kReadTimeout = 50;

private:
    /**
     * SCL and SDA of the memory, both on port B and routed to I2C2 through AF5
     */
    static constexpr uint16_t kSclPin = GPIO_PIN_13;
    static constexpr uint16_t kSdaPin = GPIO_PIN_14;

    uint8_t i2c_address_ = kDefaultI2cAddress;
    I2C_HandleTypeDef i2c_{};

    size_t size_;
    size_t address_size_;
    size_t page_size_;

    // Timeout of a single address probe while polling, generous because a NACK comes back within microseconds.
    static constexpr uint32_t kPollTimeout = 2;

    // How long a page write cycle may take before the memory counts as gone, several times its 5 ms rating.
    static constexpr uint32_t kWriteCycleTimeout = 20;

    /**
     * State of the write started with StartWrite()
     */
    struct PendingWrite
    {
        const uint8_t *data = nullptr; ///< nullptr when no write is in progress
        uint16_t address = 0;
        size_t length = 0;
        size_t written = 0;
        size_t page_length = 0; ///< Length of the page whose write cycle is running, 0 if none
        uint32_t cycle_start_ms = 0;
    };

    PendingWrite write_{};

    /**
     * HAL's word address width matching address_size_
     */
    uint16_t MemAddressSize() const;

    /**
     * 8-bit form of the device address, which is what the HAL expects
     */
    uint16_t DeviceAddress() const
    {
        return static_cast<uint16_t>(i2c_address_ << 1);
    }

    /**
     * Whether the memory already holds these bytes, at most one page
     */
    bool PageMatches(uint16_t address, const uint8_t *data, size_t len);
};
} // namespace badger
