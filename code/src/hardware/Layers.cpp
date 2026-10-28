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

#include "Layers.hpp"
#include "Badger.hpp"
#include "Buttons.hpp"

namespace badger
{
void Layers::Update()
{
    using Button = Buttons::Button;

    // Update the last layer to the current one before any changes
    last_layer_ = current_layer_;

    // Start with the current layer as the default change target
    Layer current = current_layer_;
    Layer change_to = current;

    // Snapshot freeze state before layer handling can re-freeze pots on release.
    // Also freeze on long time inside layer
    if (
        !Badger::analog_inputs.AllPotsFrozen() ||
        (current != Layer::NORMAL && GetTimeSinceLastChange() > kLayerChangeTimeMs))
    {
        disable_next_button_event_ = true;
    }

    // User forced layer change
    if (manual_change_to_ != Layer::COUNT)
    {
        change_to = manual_change_to_;
        manual_change_to_ = Layer::COUNT;
    }

    if (current == Layer::NORMAL)
    {
        if (Badger::buttons.JustPressed(Button::X))
        {
            change_to = Layer::X;
        }
        else if (Badger::buttons.JustPressed(Button::Y))
        {
            change_to = Layer::Y;
        }
        else if (Badger::buttons.JustPressed(Button::Z))
        {
            change_to = Layer::Z;
        }
    }
    else if (
        (current == Layer::X && Badger::buttons.JustReleased(Button::X)) ||
        (current == Layer::Y && Badger::buttons.JustReleased(Button::Y)) ||
        (current == Layer::Z && Badger::buttons.JustReleased(Button::Z)))
    {
        // Releasing the layer button returns to normal layer
        change_to = Layer::NORMAL;
    }
    else if (
        (current == Layer::X && Badger::buttons.JustReleased(Button::Y)) ||
        (current == Layer::X && Badger::buttons.JustReleased(Button::Z)) ||
        (current == Layer::Y && Badger::buttons.JustReleased(Button::X)) ||
        (current == Layer::Y && Badger::buttons.JustReleased(Button::Z)) ||
        (current == Layer::Z && Badger::buttons.JustReleased(Button::X)) ||
        (current == Layer::Z && Badger::buttons.JustReleased(Button::Y)))
    {
        // Button actions inside layers cancel release of the layer button event
        disable_next_button_event_ = true;
    }

    // Version layer handling
    if (Badger::buttons.PressedOver(Button::X, kHoldToEnterVersion) &&
        Badger::buttons.PressedOver(Button::Y, kHoldToEnterVersion) &&
        Badger::buttons.PressedOver(Button::Z, kHoldToEnterVersion))
    {
        change_to = Layer::VERSION;
    }

    if (change_to != current)
    {
        current_layer_ = change_to;
        Badger::analog_inputs.FreezePots();

        // Re-roll button states to prevent "just released" actions after layer change.
        if (disable_next_button_event_)
        {
            Badger::buttons.Update();
        }

        disable_next_button_event_ = false;
        last_layer_change_time_ = HAL_GetTick();
    }
};
}