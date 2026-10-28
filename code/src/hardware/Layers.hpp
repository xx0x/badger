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
#include <cstdint>
#include "py32f0xx_hal.h"

namespace badger
{

/**
 * @brief UI layer-state controller managing temporary layer switching behavior.
 * @author Vaclav Mach
 * @date 2026-06-09
 */
class Layers
{
public:
    /**
     * @brief Layers correspond to the buttons. Normal means no button is held.
     */
    enum class Layer
    {
        NORMAL,
        X,
        Y,
        Z,
        VERSION,
        COUNT
    };

    /**
     * @brief Handles layer switching mechanism.
     */
    void Update();

    /**
     * @brief Returns the current active layer.
     */
    Layer GetCurrent() const
    {
        return current_layer_;
    }

    /**
     * @brief Returns the last Update call layer - not always the "previous" layer.
     */
    Layer GetLast() const
    {
        return last_layer_;
    }

    /**
     * @brief Changes the current layer to the specified layer (in the next Update() call).
     * @param layer The layer to change to.
     */
    void ChangeTo(Layer layer)
    {
        manual_change_to_ = layer;
    }

    /**
     * @brief How many ms must the user hold a layer button to show the layer LEDs and change settings (without turning the pots). Turning the pots will trigger layer indication immediately.
     */
    static constexpr uint32_t kLayerChangeTimeMs = 500;

    /**
     * @brief How long is the LED blink when changing layers. Helps user to see that the layer has changed.
     */
    static constexpr uint32_t kLedBlinkTimeMs = 20;

    /**
     * @brief How long must the user hold all buttons to show the version.
     */
    static constexpr uint32_t kHoldToEnterVersion = 3000;

    /**
     * @brief Whether to show the layer LEDs (X/Y/Z).
     * @return True if the layer LEDs should be shown.
     */
    bool ShowXyzLeds()
    {
        return disable_next_button_event_ && (current_layer_ == Layer::X ||
                                              current_layer_ == Layer::Y ||
                                              current_layer_ == Layer::Z);
    }

    /**
     * @brief Whether to show the normal layer LEDs (preset colors).
     * @return True if the normal layer LEDs should be shown.
     */
    bool ShowNormalLeds()
    {
        return (current_layer_ == Layer::NORMAL && GetTimeSinceLastChange() > kLedBlinkTimeMs) ||
               ((current_layer_ == Layer::X ||
                 current_layer_ == Layer::Y ||
                 current_layer_ == Layer::Z) &&
                GetTimeSinceLastChange() < kLayerChangeTimeMs - kLedBlinkTimeMs);
    }

    /**
     * @brief How many milliseconds have passed since the last layer change.
     * @return Time in milliseconds since the last layer change.
     */
    uint32_t GetTimeSinceLastChange()
    {
        // Handle overflow
        // I'm wondering who's gonna run Badger over 49 days)))
        if (HAL_GetTick() < last_layer_change_time_)
        {
            return 0;
        }

        // Return time since last change in milliseconds
        return HAL_GetTick() - last_layer_change_time_;
    }

private:
    Layer current_layer_ = Layer::NORMAL;
    Layer last_layer_ = Layer::NORMAL;
    bool disable_next_button_event_ = false;
    uint32_t last_layer_change_time_ = 0;
    Layer manual_change_to_ = Layer::COUNT;
};

} // namespace badger