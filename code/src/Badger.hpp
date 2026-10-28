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

#include "hardware/AT24C.hpp"
#include "hardware/AnalogInputs.hpp"
#include "hardware/AudioInterface.hpp"
#include "hardware/Buttons.hpp"
#include "hardware/Connectivity.hpp"
#include "hardware/Layers.hpp"
#include "hardware/Leds.hpp"
#include "hardware/MidiHandler.hpp"
#include "hardware/UsbMidiHandler.hpp"

#include "app/Application.hpp"
#include "common/Version.hpp"

namespace badger
{

/**
 * @brief Initializes and manages the Badger hardware and application. Provides static access to the hardware and application instances.
 * @author Vaclav Mach
 * @date 2026-06-04
 */
class Badger
{
public:
    /**
     * @brief Version information for the Badger firmware.
     * @details The version class automatically parses it into colors during compilation.
     */
    static constexpr Version version = Version::FromString("0.1.0");

    /**
     * @brief Initializes the Badger hardware and application. Must be called before any other methods.
     */
    static void Init();

    /**
     * @brief Starts the audio processing. Must be called after Init() and before entering the main loop.
     */
    static void StartAudio();

    /**
     * @brief Handles the inputs, MIDI, layers, LEDs... Should be called inside the while(true) loop in the main().
     */
    static void UiLoop();

    /**
     * @brief Handles the audio processing, called from the audio interrupt, don't call it directly.
     */
    static void AudioLoop(AudioInterface::Buffer output_buffer);

    /**
     * @brief Call this when a fatal error occurs. Traps the CPU to halt execution.
     * @note You can also set a breakpoint here to catch errors during debugging.
     */
    static inline void ErrorHandler()
    {
        __builtin_trap();
    }

    /**
     * @brief Access to global Leds instance.
     */
    static inline Leds leds;

    /**
     * @brief Access to global Buttons instance.
     */
    static inline Buttons buttons;

    /**
     * @brief Access to global Connectivity instance.
     */
    static inline Connectivity connectivity;

    /**
     * @brief Access to global AnalogInputs instance.
     */
    static inline AnalogInputs analog_inputs;

    /**
     * @brief Access to global AudioInterface instance.
     */
    static inline AudioInterface audio;

    /**
     * @brief Access to global MidiHandler instance (TRS MIDI).
     */
    static inline MidiHandler midi;

    /**
     * @brief Access to global UsbMidiHandler instance (USB MIDI).
     */
    static inline UsbMidiHandler usb_midi;

    /**
     * @brief Access to global EEPROM instance, holding the persistent settings.
     */
    static inline AT24C eeprom = AT24C::AT24C02();

    /**
     * @brief Access to global Layers instance.
     */
    static inline Layers layers;

    /**
     * @brief Access to global Application instance.
     */
    static inline Application app;

private:
    /**
     * @brief Selects which oscillator drives the system clock (via PLL, x2, to 48MHz).
     * @note 48MHz is deliberate rather than the 72MHz the F071 can reach with PLL x3:
     *       the USB device clock is derived from the PLL and needs exactly 48MHz.
     */
    enum class SystemClock
    {
        HSE, ///< External crystal oscillator (24MHz crystal wired to PF0 and PF1)
        HSI, ///< Internal RC oscillator, calibrated to 24MHz
    };

    /**
     * @brief Configures the system clock to run from the given source.
     */
    template <SystemClock system_clock>
    static void SystemClockConfig();
};
} // namespace badger