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

#include "hardware/AT24C.hpp"

#include <algorithm>
#include <array>

namespace badger
{

bool AT24C::Init(uint8_t i2c_address)
{
    GPIO_InitTypeDef gpio_init{};

    i2c_address_ = i2c_address;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();

    // Open drain, the board carries the pull-ups.
    gpio_init.Pin = kSclPin | kSdaPin;
    gpio_init.Mode = GPIO_MODE_AF_OD;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF5_I2C2;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    i2c_.Instance = I2C2;
    i2c_.Init.ClockSpeed = kI2cClockSpeed;
    i2c_.Init.DutyCycle = I2C_DUTYCYCLE_2;
    i2c_.Init.OwnAddress1 = 0u;
    i2c_.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    i2c_.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    i2c_.Init.OwnAddress2 = 0u;
    i2c_.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c_.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    return HAL_I2C_Init(&i2c_) == HAL_OK;
}

bool AT24C::IsConnected()
{
    return HAL_I2C_IsDeviceReady(&i2c_, DeviceAddress(), 1U, kReadTimeout) == HAL_OK;
}

uint16_t AT24C::MemAddressSize() const
{
    return (address_size_ >= 2U) ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT;
}

bool AT24C::Read(uint16_t address, uint8_t *data, size_t len)
{
    if ((data == nullptr) || (len == 0u) || ((static_cast<size_t>(address) + len) > size_))
    {
        return false;
    }

    // Sequential reads roll across page boundaries by themselves, so one transfer is enough.
    return HAL_I2C_Mem_Read(&i2c_,
                            DeviceAddress(),
                            address,
                            MemAddressSize(),
                            data,
                            static_cast<uint16_t>(len),
                            kReadTimeout) == HAL_OK;
}

bool AT24C::ReadByte(uint16_t address, uint8_t *data)
{
    return Read(address, data, 1U);
}

bool AT24C::Write(uint16_t address, const uint8_t *data, size_t len)
{
    if (!StartWrite(address, data, len))
    {
        return false;
    }

    WriteStatus status;
    do
    {
        status = ProcessWrite();
    } while (status == WriteStatus::BUSY);

    return status == WriteStatus::DONE;
}

bool AT24C::WriteByte(uint16_t address, uint8_t data)
{
    return Write(address, &data, 1U);
}

bool AT24C::StartWrite(uint16_t address, const uint8_t *data, size_t len)
{
    if ((data == nullptr) || (len == 0u) || ((static_cast<size_t>(address) + len) > size_))
    {
        return false;
    }

    write_ = PendingWrite{.data = data, .address = address, .length = len};
    return true;
}

AT24C::WriteStatus AT24C::ProcessWrite()
{
    if (write_.data == nullptr)
    {
        return WriteStatus::DONE;
    }

    // A page has been sent: the memory NACKs its own address until its write cycle is over, so a single
    // address probe tells whether it is done, without waiting out a fixed delay
    if (write_.page_length > 0u)
    {
        if (HAL_I2C_IsDeviceReady(&i2c_, DeviceAddress(), 1U, kPollTimeout) != HAL_OK)
        {
            if (HAL_GetTick() - write_.cycle_start_ms > kWriteCycleTimeout)
            {
                write_.data = nullptr;
                return WriteStatus::FAILED;
            }
            return WriteStatus::BUSY;
        }

        write_.written += write_.page_length;
        write_.page_length = 0u;
    }

    if (write_.written >= write_.length)
    {
        write_.data = nullptr;
        return WriteStatus::DONE;
    }

    const uint16_t chunk_address = static_cast<uint16_t>(write_.address + write_.written);
    const uint8_t *chunk = write_.data + write_.written;

    // Writes wrap around inside a page instead of spilling into the next one.
    const size_t room_in_page = page_size_ - (chunk_address % page_size_);
    const size_t chunk_len = std::min(room_in_page, write_.length - write_.written);

    // Skip pages that already hold what we are about to write. Most saves only touch a couple
    // of bytes, so this turns a full-record write into one or two actual write cycles.
    if (PageMatches(chunk_address, chunk, chunk_len))
    {
        write_.written += chunk_len;
        return WriteStatus::BUSY;
    }

    if (HAL_I2C_Mem_Write(&i2c_,
                          DeviceAddress(),
                          chunk_address,
                          MemAddressSize(),
                          const_cast<uint8_t *>(chunk),
                          static_cast<uint16_t>(chunk_len),
                          kWriteTimeout) != HAL_OK)
    {
        write_.data = nullptr;
        return WriteStatus::FAILED;
    }

    write_.page_length = chunk_len;
    write_.cycle_start_ms = HAL_GetTick();
    return WriteStatus::BUSY;
}

bool AT24C::PageMatches(uint16_t address, const uint8_t *data, size_t len)
{
    std::array<uint8_t, kAT24C32PageSize> current{};
    if (len > current.size())
    {
        return false;
    }

    return HAL_I2C_Mem_Read(&i2c_,
                            DeviceAddress(),
                            address,
                            MemAddressSize(),
                            current.data(),
                            static_cast<uint16_t>(len),
                            kReadTimeout) == HAL_OK &&
           std::equal(data, data + len, current.begin());
}
} // namespace badger
