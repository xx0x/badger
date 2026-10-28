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


#include "hardware/MidiHandler.hpp"

#include "utils/no_interrupts_block.hpp"

#include "py32f0xx_hal.h"
#include "py32f071_hal_gpio_ex.h"

namespace badger
{

MidiHandler *MidiHandler::instance_ = nullptr;

extern "C" void USART1_IRQHandler(void)
{
    MidiHandler::IrqHandler();
}

bool MidiHandler::Init()
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {};
    gpio_init.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF0_USART1;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    uart_.Instance = USART1;
    uart_.Init.BaudRate = kMidiBaudRate;
    uart_.Init.WordLength = UART_WORDLENGTH_8B;
    uart_.Init.StopBits = UART_STOPBITS_1;
    uart_.Init.Parity = UART_PARITY_NONE;
    uart_.Init.Mode = UART_MODE_TX_RX;
    uart_.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart_.Init.OverSampling = UART_OVERSAMPLING_16;
    uart_.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&uart_) != HAL_OK)
    {
        return false;
    }

    instance_ = this;
    rx_head_ = 0;
    rx_tail_ = 0;
    tx_head_ = 0;
    tx_tail_ = 0;

    __HAL_UART_ENABLE_IT(&uart_, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&uart_, UART_IT_ERR);
    __HAL_UART_DISABLE_IT(&uart_, UART_IT_TXE);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    parser_.Reset();
    parser_.SetForwardCallback([this](const Midi::Message &message) {
        ForwardMessage(message);
    });

    merger_.Reset();
    merger_.SetSink([this](const uint8_t *data, size_t length) {
        return TxAppend(data, length);
    });

    return true;
}

void MidiHandler::EnablePassThrough(bool enable)
{
    no_interrupts_block([&]() {
        passthrough_enabled_ = enable;

        if (!passthrough_enabled_)
        {
            tx_head_ = 0;
            tx_tail_ = 0;
            __HAL_UART_DISABLE_IT(&uart_, UART_IT_TXE);
            merger_.Reset();
        }
        else if (tx_tail_ != tx_head_)
        {
            __HAL_UART_ENABLE_IT(&uart_, UART_IT_TXE);
        }
    });
}

void MidiHandler::SetPassThroughCallback(PassThroughCallback callback)
{
    passthrough_callback_ = callback;
}

void MidiHandler::ForwardMessage(const Midi::Message &message)
{
    if (passthrough_enabled_ && passthrough_callback_ != nullptr)
    {
        passthrough_callback_(message);
    }
}

void MidiHandler::IrqHandler()
{
    if (instance_ == nullptr)
    {
        return;
    }

    auto *self = instance_;

    // Continue pending passthrough TX when UART can accept data.
    if (__HAL_UART_GET_IT_SOURCE(&self->uart_, UART_IT_TXE) &&
        __HAL_UART_GET_FLAG(&self->uart_, UART_FLAG_TXE))
    {
        if (self->tx_tail_ != self->tx_head_)
        {
            self->uart_.Instance->DR = self->tx_buffer_[self->tx_tail_];
            self->tx_tail_ = static_cast<uint8_t>((self->tx_tail_ + 1U) % kTxRingBufferSize);
        }
        else
        {
            __HAL_UART_DISABLE_IT(&self->uart_, UART_IT_TXE);
        }
    }

    // Drain RX data and clear UART error conditions in a single SR/DR pass.
    while (true)
    {
        const uint32_t status = self->uart_.Instance->SR;
        const bool has_rx = (status & USART_SR_RXNE) != 0u;
        const bool has_error = (status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE)) != 0u;

        if (!has_rx && !has_error)
        {
            break;
        }

        const uint8_t byte = static_cast<uint8_t>(self->uart_.Instance->DR & 0xFFU);

        if (has_rx)
        {
            const uint8_t next_head = static_cast<uint8_t>((self->rx_head_ + 1U) % kRxRingBufferSize);
            if (next_head == self->rx_tail_)
            {
                // Ring full: drop oldest byte to keep newest stream aligned.
                self->rx_tail_ = static_cast<uint8_t>((self->rx_tail_ + 1U) % kRxRingBufferSize);
            }
            self->rx_buffer_[self->rx_head_] = byte;
            self->rx_head_ = next_head;

            // Real-time bytes are passed through right here to keep clock jitter low, they may go anywhere
            // in the output stream. Everything else is merged message by message in Update().
            if (self->passthrough_enabled_ && byte >= static_cast<uint8_t>(Midi::Type::CLOCK))
            {
                self->TxAppend(&byte, 1);
            }
        }
    }
}

void MidiHandler::SetRealTimeMessageCallback(RealTimeMessageCallback callback)
{
    parser_.SetRealTimeMessageCallback(callback);
}

void MidiHandler::SetChannelMessageCallback(ChannelMessageCallback callback)
{
    parser_.SetChannelMessageCallback(callback);
}

void MidiHandler::SetChannel(uint8_t channel)
{
    parser_.SetChannel(channel);
}

uint8_t MidiHandler::GetChannel() const
{
    return parser_.GetChannel();
}

void MidiHandler::Update()
{
    // Parsing drives both the application callbacks and the USB OUT passthrough.
    // The TRS OUT passthrough goes through the merger, except for real-time bytes, which the RX interrupt already passed on.
    while (rx_tail_ != rx_head_)
    {
        const uint8_t byte = rx_buffer_[rx_tail_];
        rx_tail_ = static_cast<uint8_t>((rx_tail_ + 1U) % kRxRingBufferSize);
        parser_.ParseByte(byte);

        if (passthrough_enabled_ && byte < static_cast<uint8_t>(Midi::Type::CLOCK))
        {
            merger_.Write(MidiMerger::Input::TRS, byte);
        }
    }

    merger_.Process(HAL_GetTick());
}

void MidiHandler::SendNoteOn(uint8_t note, uint8_t velocity)
{
    Midi::Message message = {Midi::Type::NOTE_ON, GetChannel(), note, velocity};
    Send(message);
}

void MidiHandler::SendNoteOff(uint8_t note, uint8_t velocity)
{
    Midi::Message message = {Midi::Type::NOTE_OFF, GetChannel(), note, velocity};
    Send(message);
}

void MidiHandler::SendRealTime(Midi::Type type)
{
    Midi::Message message = {type, 0, 0, 0};
    Send(message);
}

void MidiHandler::Send(Midi::Message message)
{
    uint8_t bytes[MidiParser::kMaxMessageBytes] = {};
    const size_t total = MidiParser::Encode(message, bytes);

    merger_.Write(MidiMerger::Input::LOCAL, bytes, total);
}

void MidiHandler::ForwardFromUsb(const uint8_t *data, size_t length)
{
    if (data == nullptr)
    {
        return;
    }

    merger_.Write(MidiMerger::Input::USB, data, length);
}

bool MidiHandler::TxAppend(const uint8_t *data, size_t length)
{
    return no_interrupts_block([&]() {
        // All or nothing, a full ring drops whole messages rather than splitting them
        const size_t used = (tx_head_ + kTxRingBufferSize - tx_tail_) % kTxRingBufferSize;
        if (length > kTxRingBufferSize - 1U - used)
        {
            return false;
        }

        for (size_t i = 0; i < length; ++i)
        {
            tx_buffer_[tx_head_] = data[i];
            tx_head_ = static_cast<uint8_t>((tx_head_ + 1U) % kTxRingBufferSize);
        }

        if (__HAL_UART_GET_FLAG(&uart_, UART_FLAG_TXE) && tx_tail_ != tx_head_)
        {
            uart_.Instance->DR = tx_buffer_[tx_tail_];
            tx_tail_ = static_cast<uint8_t>((tx_tail_ + 1U) % kTxRingBufferSize);
        }

        if (tx_tail_ != tx_head_)
        {
            __HAL_UART_ENABLE_IT(&uart_, UART_IT_TXE);
        }

        return true;
    });
}

} // namespace badger
