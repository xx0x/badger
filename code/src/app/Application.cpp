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

#include "Application.hpp"
#include "Badger.hpp"
#include "dsp/math/HyperRandomness.hpp"
#include "dsp/sampling/EightBitSamplePlayer.hpp"
#include "samples/samples_badger.hpp"
#include "utils/EnumTools.hpp"
#include "utils/no_interrupts_block.hpp"

namespace badger
{

#define MEASURE_AUDIO_LOOP false
#define SEND_SCALE_CC false
#define SEND_ROOT_NOTE_CC false

#if VERSION_PLAYS_BADGER
// VERSION_PLAYS_BADGER is defined in Application.hpp
// Lower these values if the app doesn't fit into flash (or disable VERSION_PLAYS_BADGER altogether)
static constexpr size_t BADGER_VERSION_SAMPLE_RATE = 8000;
static constexpr uint8_t BADGER_VERSION_SAMPLE_BIT_DEPTH = 6;
#endif

bool Application::Init()
{
    clock_.Init(Badger::audio.GetCallbackRate());
    sequencer_.Init();
    presets_.GetDsp().Init(Badger::audio.GetSampleRate(),
                           Badger::audio.GetCallbackRate());

    presets_.GetParams().notes.fill(0);
    presets_.GetParams().triggers.fill(false);

    presets_.Init();

#if VERSION_PLAYS_BADGER
    static constexpr auto kVersionSample = EightBitSamplePlayer::Compress<BADGER_VERSION_SAMPLE_RATE, BADGER_VERSION_SAMPLE_BIT_DEPTH, 8000>(samples_badger);
    version_sample_player_.Init(Badger::audio.GetSampleRate(), kVersionSample.sample_rate, kVersionSample.bit_depth);
    version_sample_player_.Load(kVersionSample.data, kVersionSample.sample_count);
#endif

    if (Badger::buttons.PressedRaw(Buttons::Button::Y))
    {
        FactoryReset();
    }
    else
    {
        LoadSavedSettings();
    }

    Badger::midi.EnablePassThrough(true);

    // TRS IN -> USB OUT. TRS IN -> TRS OUT happens inside the MIDI handler itself.
    Badger::midi.SetPassThroughCallback([](const Midi::Message &message) {
        Badger::usb_midi.Send(message);
    });

    // USB IN -> TRS OUT. There is deliberately no USB IN -> USB OUT, which would loop back to the host.
    Badger::usb_midi.SetPassThroughCallback([](const uint8_t *data, size_t length) {
        Badger::midi.ForwardFromUsb(data, length);
    });

    HyperRandomness::Seed(Badger::analog_inputs.GatherEntropy());

    Badger::midi.SetRealTimeMessageCallback([this](const Midi::Message &message) {
        HandleMidiRealTimeMessage(MidiPort::TRS, message);
    });
    Badger::midi.SetChannelMessageCallback([this](const Midi::Message &message) {
        HandleMidiChannelMessage(message);
    });

    Badger::usb_midi.SetRealTimeMessageCallback([this](const Midi::Message &message) {
        HandleMidiRealTimeMessage(MidiPort::USB, message);
    });
    Badger::usb_midi.SetChannelMessageCallback([this](const Midi::Message &message) {
        HandleMidiChannelMessage(message);
    });

    return true;
}

void Application::FactoryReset()
{
    // Force reset saved settings to defaults if a button is held during startup
    save_file_dirty_ = false;
    save_file_ = {};
    save_file_.preset_pots_storage = presets_.GetPresetPotStorage();
    ApplySaveFile();

    bool success = save_file_handler_.SaveNow(save_file_);
    Color color = success ? Color::GREEN : Color::RED;
    while (Badger::buttons.PressedRaw(Buttons::Button::Y))
    {
        // This loop owns the CPU for as long as the buttons are held, so keep servicing USB
        // or the host would time out mid-enumeration.
        Badger::usb_midi.Update();

        Badger::leds.Set(0, color);
        Badger::leds.Set(1, color);
        Badger::leds.Set(2, color);
        Badger::leds.Update();
        HAL_Delay(200);
        Badger::leds.Set(0, Color::BLACK);
        Badger::leds.Set(1, Color::BLACK);
        Badger::leds.Set(2, Color::BLACK);
        Badger::leds.Update();
        HAL_Delay(200);

        Badger::buttons.Update();
    }
}

void Application::LoadSavedSettings()
{
    // Load saved settings from the EEPROM, keeping the defaults if there is nothing valid there
    if (const auto loaded_save = save_file_handler_.Load(); loaded_save.has_value())
    {
        save_file_ = *loaded_save;
    }

    ApplySaveFile();
}

void Application::ApplySaveFile()
{
    if (save_file_.volume >= 0 && save_file_.volume <= Q15_MAX)
    {
        volume_ = save_file_.volume;
    }

    if (save_file_.internal_clock_pulse_period > 0)
    {
        clock_.SetInternalPulsePeriod(save_file_.internal_clock_pulse_period);
    }

    if (save_file_.midi_clock_divider_index < Clock::kMidiDividerOptions.size())
    {
        clock_.SetMidiDividerIndex(save_file_.midi_clock_divider_index);
    }

    if (save_file_.external_clock_divider_multiplier_index < Clock::kExternalDividerMultiplierOptions.size())
    {
        clock_.SetExtMultiplierIndex(save_file_.external_clock_divider_multiplier_index);
    }

    if (save_file_.root_note < TrackSequencer::kRootNotes)
    {
        sequencer_.SetRootNote(save_file_.root_note);
    }
    if (save_file_.scale_index < TrackSequencer::kScales.size())
    {
        sequencer_.SetScaleIndex(save_file_.scale_index);
    }

    presets_.SetPresetPotsStorage(save_file_.preset_pots_storage);
    SetMidiChannel(save_file_.midi_channel);
}

void Application::Randomize()
{
    using Pot = AnalogInputs::Pot;

    HyperRandomness::AddEntropy(Badger::analog_inputs.GatherEntropy());

    auto randomize_type = presets_.GetCurrent().GetRandomizeType();

    if (randomize_type == Preset::RandomizeType::RHYTHM_MELODY ||
        randomize_type == Preset::RandomizeType::RHYTHM)
    {
        sequencer_.RandomizeRhythmTrackSet();
    }

    if (randomize_type == Preset::RandomizeType::RHYTHM_MELODY ||
        randomize_type == Preset::RandomizeType::MELODY)
    {
        auto octave_span = presets_.GetCurrent().GetRandomizerOctaveSpan();
        sequencer_.RandomizeMelodyTrackSet(octave_span.lower, octave_span.upper);
    }

    if (randomize_type == Preset::RandomizeType::GLOBAL_POTS ||
        randomize_type == Preset::RandomizeType::ALL_POTS)
    {
        for (auto &pot : presets_.GetParams().global_pots)
        {
            pot = HyperRandomness::NextRange(0, AnalogInputs::kMaxValue + 1);
        }
    }

    if (randomize_type == Preset::RandomizeType::PATCH_POTS ||
        randomize_type == Preset::RandomizeType::ALL_POTS)
    {
        EnumArray<Pot, std::optional<uint16_t>> new_preset_pots;
        for (auto &pot : new_preset_pots)
        {
            pot = HyperRandomness::NextRange(0, AnalogInputs::kMaxValue + 1);
        }
        presets_.GetCurrent().UpdatePresetPots(new_preset_pots);
    }
}

void Application::LoadLightSensors()
{
    using LightSensor = AnalogInputs::LightSensor;

    // Load light sensor values into the shared params
    for (const auto light_sensor : EnumRange<LightSensor>())
    {
        q15_t val = Badger::analog_inputs.GetLightSensorValue(light_sensor, Q15_ZERO, Q15_MAX, false);
        val = q15_saturate(val);
        presets_.GetParams().light_sensors[light_sensor] = val;
    }
}

void Application::UpdatePitchBend()
{
    presets_.GetParams().pitch_bend = midi_pitch_bend_semitones_ + Badger::analog_inputs.GetCvInSemitones();
}

bool Application::RegularOperation() const
{
#if VERSION_PLAYS_BADGER
    return Badger::layers.GetCurrent() != Layers::Layer::VERSION;
#else
    return true;
#endif
}

void Application::UiLoop()
{
    using Button = Buttons::Button;
    using Layer = Layers::Layer;
    using Pot = AnalogInputs::Pot;

    save_file_handler_.Process();

    ProcessPendingPresetCc();

    ScrollPresets();

    auto params = no_interrupts_block([&]() {
        // Make a copy of the params to avoid issues with them being changed in the audio loop while we're using them here
        auto copied_params = presets_.GetParams();

        // Fill in the incoming MIDI note, and then clear events
        copied_params.incoming_midi_note = incoming_midi_note_;
        incoming_midi_note_.note_on = false;
        incoming_midi_note_.note_off = false;

        // Clear the triggers after copying, so they will only be true for one UI loop
        for (auto track : EnumRange<TrackSequencer::Track>())
        {
            presets_.GetParams().triggers[track] = false;
        }

        return copied_params;
    });

    // Load light sensor values into the shared params
    LoadLightSensors();

    // Combine MIDI pitch bend with the CV input (v/oct) into the shared params
    UpdatePitchBend();

    // In regular operation update the preset
    if (RegularOperation())
    {
        // Update the current preset's parameters with the shared params
        presets_.GetCurrent().UpdateParams(params);
    }

    // Read common values
    auto alpha_pot = Badger::analog_inputs.ReadPot(Pot::ALPHA);
    auto beta_pot = Badger::analog_inputs.ReadPot(Pot::BETA);
    auto gamma_pot = Badger::analog_inputs.ReadPot(Pot::GAMMA);

    const EnumArray<Pot, std::optional<uint16_t>> pot_values = {alpha_pot, beta_pot, gamma_pot};

    // If a pot was frozen by MIDI CC and then moved by hand, release CC override and hand control back.
    for (auto pot : EnumRange<Pot>())
    {
        if (!pot_values[pot].has_value())
        {
            continue;
        }

        const uint16_t pot_value = pot_values[pot].value();

        if (global_pot_cc_active_[pot])
        {
            global_pot_cc_active_[pot] = false;
            presets_.GetParams().global_pots[pot] = pot_value;
        }

        if (preset_pot_cc_active_[pot])
        {
            preset_pot_cc_active_[pot] = false;
            presets_.GetCurrent().SetPresetPot(pot, pot_value);
        }
    }

    // Layers
    const auto layer = Badger::layers.GetCurrent();
    const auto last_layer = Badger::layers.GetLast();
    bool show_xyz_leds = Badger::layers.ShowXyzLeds();
    bool show_normal_leds = Badger::layers.ShowNormalLeds();

    // Preset colors
    Color preset_color = presets_.GetCurrent().GetColor();
    Color group_color = presets_.GetCurrent().GetGroupColor();

    if (show_normal_leds)
    {
        uint8_t brightness = presets_.GetCurrent().GetLedBrightness();
        Color preset_color_with_brightness = ApplyBrightness(preset_color, brightness);
        Color preset_color_with_inverted_brightness = ApplyBrightness(preset_color, (255u - brightness) / 2);
        Color group_color_with_brightness = ApplyBrightness(group_color, brightness);

        Badger::leds.Set(0, preset_color_with_brightness);
        Badger::leds.Set(1, MixColors(preset_color_with_inverted_brightness, group_color_with_brightness));
        Badger::leds.Set(2, preset_color_with_brightness);
    }

    // No leds
    if (!show_normal_leds && !show_xyz_leds)
    {
        Badger::leds.Set(0, Color::BLACK);
        Badger::leds.Set(1, Color::BLACK);
        Badger::leds.Set(2, Color::BLACK);
    }

    // Version layer display
    if (layer == Layer::VERSION)
    {
        // Version blinking settings
        static constexpr size_t kVersionBlinkInterval = 220;
        static constexpr size_t kVersionBlinkCount = 8;

        // Reset version announcement when entering
        if (last_layer != Layer::VERSION)
        {
            version_last_time_blinked_ = 0;
            version_blink_count_ = 0;
            version_blink_state_ = false;
        }
        // Blink the version LEDs and play the version sample
        if (version_last_time_blinked_ == 0 || (HAL_GetTick() - version_last_time_blinked_) >= kVersionBlinkInterval)
        {
            version_blink_state_ = !version_blink_state_;
            version_last_time_blinked_ = HAL_GetTick();
            if (version_blink_state_)
            {
                if (version_blink_count_ >= kVersionBlinkCount)
                {
                    Badger::layers.ChangeTo(Layer::NORMAL);
                }
                else
                {
                    version_blink_count_++;
#if VERSION_PLAYS_BADGER
                    version_sample_player_.Trigger();
#endif
                }
            }
        }
        // Update the version LEDs
        auto &version_colors = Badger::version.GetColorCode();
        for (size_t i = 0; i < version_colors.size(); ++i)
        {
            Badger::leds.Set(i, version_blink_state_ ? version_colors[i] : Color::BLACK);
        }
    }

    if (layer == Layer::NORMAL)
    {
        for (auto pot : EnumRange<Pot>())
        {
            const bool has_hardware_value = pot_values[pot].has_value();
            const bool has_cc_value = global_pot_cc_active_[pot];
            if (!has_hardware_value && !has_cc_value)
            {
                continue;
            }

            const uint16_t pot_value = !has_cc_value ? pot_values[pot].value() : presets_.GetParams().global_pots[pot];
            presets_.GetParams().global_pots[pot] = pot_value;

            if (pot == Pot::ALPHA)
            {
                size_t rhythm_set = pot_to_range(pot_value, TrackSequencer::kRhythmTrackSetsCount - 1);
                sequencer_.SetRhythmTrackSet(rhythm_set);
            }
            else if (pot == Pot::GAMMA)
            {
                size_t melody_set = pot_to_range(pot_value, TrackSequencer::kMelodyTrackSetsCount - 1);
                sequencer_.SetMelodyTrackSet(melody_set);
            }
        }

        if (Badger::buttons.JustReleased(Button::X))
        {
            presets_.Change(-1);
            ClearPresetPotCcOverrides();
            scroll_presets_ = 0;
        }

        if (Badger::buttons.JustReleased(Button::Y))
        {
            Randomize();
        }

        if (Badger::buttons.JustReleased(Button::Z))
        {
            presets_.Change(1);
            ClearPresetPotCcOverrides();
            scroll_presets_ = 0;
        }
    }

    if (layer != Layer::X)
    {
        clock_.ClearTapTempo();
    }

    if (layer == Layer::X)
    {
        if (alpha_pot.has_value())
        {
            const ClockSource::Type active_source = clock_.GetActiveSourceType();
            const uint32_t prev_internal_period = clock_.GetInternalPulsePeriod();
            const size_t prev_midi_divider_index = clock_.GetMidiDividerIndex();
            const size_t prev_external_divider_multiplier_index = clock_.GetExtMultiplierIndex();

            clock_.SetFromPot(alpha_pot.value());

            if (active_source == ClockSource::Type::INTERNAL)
            {
                const uint32_t new_period = clock_.GetInternalPulsePeriod();
                if (new_period != prev_internal_period)
                {
                    save_file_.internal_clock_pulse_period = new_period;
                    save_file_dirty_ = true;
                }
            }
            else if (active_source == ClockSource::Type::MIDI)
            {
                const size_t new_midi_divider_index = clock_.GetMidiDividerIndex();
                if (new_midi_divider_index != prev_midi_divider_index && new_midi_divider_index < Clock::kMidiDividerOptions.size())
                {
                    save_file_.midi_clock_divider_index = new_midi_divider_index;
                    save_file_dirty_ = true;
                }
            }
            else if (active_source == ClockSource::Type::EXTERNAL)
            {
                const size_t new_external_divider_multiplier_index = clock_.GetExtMultiplierIndex();
                if (new_external_divider_multiplier_index != prev_external_divider_multiplier_index &&
                    new_external_divider_multiplier_index < Clock::kExternalDividerMultiplierOptions.size())
                {
                    save_file_.external_clock_divider_multiplier_index = new_external_divider_multiplier_index;
                    save_file_dirty_ = true;
                }
            }
        }

        if (beta_pot.has_value())
        {
            static constexpr std::array<uint8_t, 17> kMidiChannelSelection = {Midi::OMNI_CHANNEL, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

            const size_t new_channel_index = pot_to_range(beta_pot.value(), kMidiChannelSelection.size() - 1);
            const uint8_t new_channel = kMidiChannelSelection[new_channel_index];
            if (new_channel != Badger::midi.GetChannel())
            {
                // Update the MIDI channel and save it to the EEPROM
                SetMidiChannel(new_channel);
                save_file_.midi_channel = new_channel;
                save_file_dirty_ = true;

                // Switch to internal play mode
                presets_.GetParams().play_mode = Preset::PlayMode::INTERNAL;
                midi_velocity_gain_target_ = Q15_MAX;
                midi_velocity_gain_current_ = Q15_MAX;
                midi_note_held_ = false;
            }
        }

        if (gamma_pot.has_value())
        {
            static constexpr auto kVolumeMap = MapDef<int32_t, 6>{
                {pot(0.0f), pot(0.25f), pot(0.5f), pot(0.75f), pot(0.9f), pot(1.0f)},
                {q15(0.0f), q15(0.0300f), q15(0.125f), q15(0.25f), q15(0.5f), q15(0.75f)}};
            const q15_t new_volume = curve_map(gamma_pot.value(), kVolumeMap, MapClamp::TRUE, MapSafe::FALSE);
            if (new_volume != volume_)
            {
                volume_ = new_volume;
                save_file_.volume = new_volume;
                save_file_dirty_ = true;
            }
        }

        bool running = clock_.GetState() == ClockSource::State::RUNNING;

        // JustPressed because we need precise timings
        // Doesn't cause issue with entering the VERSION layer because it needs multiple taps anyway
        if (Badger::buttons.JustPressed(Button::Y))
        {
            clock_.TapTempo();
        }

        // JustReleased because we don't want to start/stop the clock when entering VERSION
        if (Badger::buttons.JustReleased(Button::Z))
        {
            if (running)
            {
                clock_.Stop();
            }
            else
            {
                clock_.Start();
            }
        }

        if (show_xyz_leds)
        {
            Color led_color = Color::BLUE;
            if (clock_.GetActiveSourceType() == ClockSource::Type::MIDI)
            {
                led_color = Color::CYAN;
            }
            else if (clock_.GetActiveSourceType() == ClockSource::Type::EXTERNAL)
            {
                led_color = Color::MAGENTA;
            }
            else
            {
                led_color = Color::BLUE;
            }

            Color midi_channel_color = Midi::kOmniChannelColor;
            if (Badger::midi.GetChannel() <= 15)
            {
                midi_channel_color = Midi::kChannelColors[Badger::midi.GetChannel()];
            }

            Badger::leds.Set(0, clock_.GetHalfDutyOutput() ? led_color : Color::BLACK);
            Badger::leds.Set(1, Badger::buttons.Pressed(Button::Y) ? Color::BLACK : midi_channel_color);
            Badger::leds.Set(2, running ? Color::GREEN : Color::RED);
        }
    }

    if (layer == Layer::Y)
    {
        presets_.GetCurrent().UpdatePresetPots({{alpha_pot, beta_pot, gamma_pot}});

        if (Badger::buttons.JustReleased(Button::X))
        {
            scroll_presets_--;
        }

        if (Badger::buttons.JustReleased(Button::Z))
        {
            scroll_presets_++;
        }

        if (show_xyz_leds)
        {
            Badger::leds.Set(0, preset_color);
            Badger::leds.Set(1, preset_color);
            Badger::leds.Set(2, preset_color);
        }
    }
    if (layer == Layer::Z)
    {
        if (alpha_pot.has_value())
        {
            size_t new_scale_index = pot_to_range(alpha_pot.value(), TrackSequencer::kScales.size() - 1);
            if (new_scale_index != sequencer_.GetScaleIndex())
            {
                sequencer_.SetScaleIndex(new_scale_index);
                save_file_.scale_index = static_cast<uint8_t>(new_scale_index);
                save_file_dirty_ = true;
#if SEND_SCALE_CC
                const Midi::Message message = {Midi::Type::CONTROL_CHANGE,
                                               0,
                                               static_cast<uint8_t>(Midi::ControlChange::SCALE),
                                               TrackSequencer::ScaleToCc(new_scale_index)};
                Badger::midi.Send(message);
                Badger::usb_midi.Send(message);
#endif
            }
        }
        if (beta_pot.has_value())
        {
            const int32_t new_octave = pot_to_range(beta_pot.value(), TrackSequencer::kMinOctaveOffset, TrackSequencer::kMaxOctaveOffset);
            if (new_octave != sequencer_.GetOctaveOffset())
            {
                sequencer_.SetOctaveOffset(new_octave);
            }
        }
        if (gamma_pot.has_value())
        {
            size_t new_root_note = pot_to_range(gamma_pot.value(), 11u);
            if (new_root_note != sequencer_.GetRootNote())
            {
                sequencer_.SetRootNote(new_root_note);
                save_file_.root_note = static_cast<uint8_t>(new_root_note);
                save_file_dirty_ = true;
#if SEND_ROOT_NOTE_CC
                const Midi::Message message = {Midi::Type::CONTROL_CHANGE,
                                               0,
                                               static_cast<uint8_t>(Midi::ControlChange::ROOT_NOTE),
                                               TrackSequencer::RootNoteToCc(new_root_note)};
                Badger::midi.Send(message);
                Badger::usb_midi.Send(message);
#endif
            }
        }

        if (Badger::buttons.JustReleased(Button::X))
        {
            Badger::analog_inputs.FreezePot(Pot::BETA); // Freeze octave pot
            const int32_t new_octave = sequencer_.GetOctaveOffset() - 1;
            if (new_octave >= TrackSequencer::kMinOctaveOffset)
            {
                sequencer_.SetOctaveOffset(new_octave);
            }
        }

        if (Badger::buttons.JustReleased(Button::Y))
        {
            Badger::analog_inputs.FreezePot(Pot::BETA); // Freeze octave pot
            const int32_t new_octave = sequencer_.GetOctaveOffset() + 1;
            if (new_octave <= TrackSequencer::kMaxOctaveOffset)
            {
                sequencer_.SetOctaveOffset(new_octave);
            }
        }

        if (show_xyz_leds)
        {
            Badger::leds.Set(0, sequencer_.GetCurrentScaleColor());
            Badger::leds.Set(1, sequencer_.GetCurrentOctaveColor());
            Badger::leds.Set(2, sequencer_.GetCurrentRootColor());
        }
    }

    presets_.GetParams().base_note = sequencer_.GetBaseNote();
    presets_.GetParams().current_scale = sequencer_.GetCurrentScale();

    // Save preset pots when leaving the Y layer
    if (last_layer == Layer::Y && layer != Layer::Y)
    {
        SavePresetPots();
    }

    // Schedule a save operation if the save file is dirty and the layer has changed from a non-normal layer to another layer
    if (save_file_dirty_ && last_layer != layer && last_layer != Layer::NORMAL)
    {
        save_file_handler_.ScheduleSave(save_file_);
        save_file_dirty_ = false;
    }

    SendOutgoingMidi();
}

void Application::SavePresetPots()
{
    // Preset pots modified via MIDI CC are not auto-saved to prevent constant overwriting of the EEPROM.
    for (auto pot : EnumRange<AnalogInputs::Pot>())
    {
        if (preset_pot_cc_active_[pot])
        {
            return;
        }
    }

    const auto &old_preset_pots_storage = save_file_.preset_pots_storage;
    const auto &new_preset_pots_storage = presets_.GetPresetPotStorage();
    const size_t current_preset_index = presets_.GetCurrentPresetIndex();
    if (old_preset_pots_storage[current_preset_index] != new_preset_pots_storage[current_preset_index])
    {
        save_file_.preset_pots_storage = new_preset_pots_storage;
        save_file_dirty_ = true;
    }
}

void Application::ScrollPresets()
{
    if (scroll_presets_flag_)
    {
        scroll_presets_flag_ = false;
        presets_.Change(scroll_presets_);
        ClearPresetPotCcOverrides();
    }
}

void Application::ClearPresetPotCcOverrides()
{
    for (auto pot : EnumRange<AnalogInputs::Pot>())
    {
        preset_pot_cc_active_[pot] = false;
    }
}

void Application::ProcessPendingPresetCc()
{
    bool process_preset_cc = false;
    uint8_t preset_cc_value = 0;

    no_interrupts_block([&]() {
        process_preset_cc = preset_cc_pending_;
        preset_cc_value = preset_cc_value_;
        preset_cc_pending_ = false;
    });

    if (process_preset_cc)
    {
        presets_.SelectFromCc(preset_cc_value);
        ClearPresetPotCcOverrides();
        scroll_presets_ = 0;
    }
}

void Application::AudioLoop(AudioInterface::Buffer output_buffer)
{

#if MEASURE_AUDIO_LOOP
    Badger::connectivity.SetDebugPin(true);
#endif

    ClockSource::Inputs clock_inputs = ProcessIncomingMidi();
    clock_inputs.external_sync_edge = Badger::connectivity.ConsumeClockInEdge();

    auto clock_result = clock_.Process(clock_inputs);

    // Handle outgoing MIDI real-time messages
    PrepareOutgoingMidi(clock_result);

    // Handle sequencer reset when sending MIDI start
    if (clock_result.midi_start)
    {
        sequencer_.Reset();
    }

#if !MEASURE_AUDIO_LOOP
    Badger::connectivity.SetClockOut(clock_.GetHalfDutyOutput());
#endif

    sequencer_.Process(clock_result.step);

    if (clock_result.realign_to >= 0)
    {
        sequencer_.RealignTo(clock_result.realign_to);
    }

    if (clock_result.step && sequencer_.GetStep() % 4 == 0)
    {
        scroll_presets_flag_ = true;
    }

    // Schedule primary-track MIDI notes in audio loop, emit them later in UI loop.
    midi_note_sender_.ProcessPrimaryTrack(clock_result,
                                          clock_.GetState(),
                                          Badger::midi.GetChannel(),
                                          sequencer_.IsNowTrigger(TrackSequencer::Track::PRIMARY),
                                          sequencer_.GetCurrentMelodyNote(TrackSequencer::Track::PRIMARY));

    auto &params = presets_.GetParams();

    // Update triggers and notes (when internal mode is active)
    if (params.play_mode == Preset::PlayMode::INTERNAL)
    {
        for (auto track : EnumRange<TrackSequencer::Track>())
        {
            if (sequencer_.IsNowTrigger(track))
            {
                params.triggers[track] = true;
                params.notes[track] = sequencer_.GetCurrentMelodyNote(track);
            }
        }
    }

    if (RegularOperation())
    {
        // Run the normal preset audio loop
        presets_.GetCurrent().AudioLoop(output_buffer);

        // Apply volume, volume compensation, and MIDI velocity gain
        // Volume and compensation hold for the whole block, only the velocity gain slews per frame,
        // so they are combined into a single gain up front instead of multiplying every sample three times
        const q15_t block_gain = q15_mult(volume_, presets_.GetCurrent().GetVolumeCompensation());
        for (auto &[left, right] : output_buffer)
        {
            SlewMidiVelocityGain();
            const q15_t gain = q15_mult(block_gain, midi_velocity_gain_current_);
            left = q15_mult(left, gain);
            right = q15_mult(right, gain);
        }
    }
    else
    {
#if VERSION_PLAYS_BADGER
        // Play version announcement
        for (auto &frame : output_buffer)
        {
            const q15_t sample = q15_mult(version_sample_player_.Process(), volume_);
            frame = {sample, sample};
        }
#endif
    }

#if MEASURE_AUDIO_LOOP
    Badger::connectivity.SetDebugPin(false);
#endif
}

ClockSource::Inputs Application::ProcessIncomingMidi()
{
    EnumArray<MidiPort, bool> pulse = {false, false};
    EnumArray<MidiPort, bool> start = {false, false};
    EnumArray<MidiPort, bool> stop = {false, false};
    EnumArray<MidiPort, bool> cont = {false, false};

    no_interrupts_block([&]() {
        for (auto port : EnumRange<MidiPort>())
        {
            auto &incoming = incoming_midi_[port];

            if (incoming.clock_pulses > 0)
            {
                incoming.clock_pulses = incoming.clock_pulses - 1;
                pulse[port] = true;
            }

            start[port] = incoming.start;
            stop[port] = incoming.stop;
            cont[port] = incoming.cont;

            incoming.start = false;
            incoming.stop = false;
            incoming.cont = false;
        }
    });

    // Ports are iterated highest priority first. The MIDI clock source rejects transport coming
    // from a lower-priority port while a higher-priority one is driving it.
    for (auto port : EnumRange<MidiPort>())
    {
        if (start[port] && clock_.MidiStart(port))
        {
            sequencer_.Reset();
        }

        if (stop[port])
        {
            clock_.MidiStop(port);
        }

        if (cont[port])
        {
            clock_.MidiContinue(port);
        }
    }

    ClockSource::Inputs inputs;
    inputs.usb_midi_pulse = pulse[MidiPort::USB];
    inputs.trs_midi_pulse = pulse[MidiPort::TRS];
    return inputs;
}

void Application::PrepareOutgoingMidi(const ClockSource::ProcessResult &clock_result)
{
    no_interrupts_block([&]() {
        if (clock_result.midi_pulse)
        {
            outgoing_midi_.clock_pulses = outgoing_midi_.clock_pulses + 1;
        }
        if (clock_result.midi_start)
        {
            outgoing_midi_.start = true;
        }
        if (clock_result.midi_stop)
        {
            outgoing_midi_.stop = true;
        }
        if (clock_result.midi_continue)
        {
            outgoing_midi_.cont = true;
        }
    });
}

void Application::SendOutgoingMidi()
{
    bool send_start = false;
    bool send_stop = false;
    bool send_continue = false;
    bool send_clock = false;

    no_interrupts_block([&]() {
        send_start = outgoing_midi_.start;
        send_stop = outgoing_midi_.stop;
        send_continue = outgoing_midi_.cont;

        outgoing_midi_.start = false;
        outgoing_midi_.stop = false;
        outgoing_midi_.cont = false;

        if (outgoing_midi_.clock_pulses > 0)
        {
            outgoing_midi_.clock_pulses = outgoing_midi_.clock_pulses - 1;
            send_clock = true;
        }
    });

    // Everything the Badger generates goes out on both MIDI ports.
    auto send_realtime = [](Midi::Type type) {
        Badger::midi.SendRealTime(type);
        Badger::usb_midi.SendRealTime(type);
    };

    if (send_start)
    {
        send_realtime(Midi::Type::START);
    }

    if (send_stop)
    {
        send_realtime(Midi::Type::STOP);
    }

    if (send_continue)
    {
        send_realtime(Midi::Type::CONTINUE);
    }

    if (send_clock)
    {
        send_realtime(Midi::Type::CLOCK);
    }

    const auto pending_primary_track_notes = midi_note_sender_.ConsumePendingMessages();
    if (pending_primary_track_notes.send_note_off)
    {
        Badger::midi.Send(pending_primary_track_notes.note_off);
        Badger::usb_midi.Send(pending_primary_track_notes.note_off);
    }
    if (pending_primary_track_notes.send_note_on)
    {
        Badger::midi.Send(pending_primary_track_notes.note_on);
        Badger::usb_midi.Send(pending_primary_track_notes.note_on);
    }
}

void Application::HandleMidiRealTimeMessage(MidiPort port, const Midi::Message &message)
{
    no_interrupts_block([&]() {
        auto &incoming = incoming_midi_[port];

        if (message.type == Midi::Type::CLOCK)
        {
            incoming.clock_pulses = incoming.clock_pulses + 1;
        }

        if (message.type == Midi::Type::START)
        {
            incoming.start = true;
            incoming.clock_pulses = 0;
        }

        if (message.type == Midi::Type::CONTINUE)
        {
            incoming.cont = true;
        }

        if (message.type == Midi::Type::STOP)
        {
            incoming.stop = true;
            incoming.clock_pulses = 0;
        }
    });
}

void Application::SetMidiChannel(uint8_t channel)
{
    Badger::midi.SetChannel(channel);
    Badger::usb_midi.SetChannel(channel);
}

void Application::HandleMidiNoteOff(uint8_t note, uint8_t release_velocity)
{
    // Allow only the note that was previously turned on to be turned off
    // To prevent turning off a note when quickly playing multiple notes on a keyboard,
    // which can cause the last note to be turned off when releasing the first note
    if (note == incoming_midi_note_.note)
    {
        midi_note_held_ = false;
        incoming_midi_note_ = {false, true, note, release_velocity};
    }
}

void Application::HandleMidiAftertouch(uint8_t pressure)
{
    // Aftertouch reshapes only a note that is still held, so pressure arriving after the note
    // was released can't swell its release tail. It reuses the note-on velocity curve, so the
    // pressure and the velocity of a note map to the very same output gain.
    if (!midi_note_held_)
    {
        return;
    }

    // Single-shot presets have decayed by the time pressure builds up, so they opt out entirely.
    if (!presets_.GetCurrent().AllowAftertouch())
    {
        return;
    }

    // Only the target moves, so the audio loop ramps into the new pressure rather than stepping to it.
    midi_velocity_gain_target_ = VelocityToGain(pressure);
}

void Application::HandleMidiChannelMessage(const Midi::Message &message)
{
    if (message.type == Midi::Type::PROGRAM_CHANGE)
    {
        // Program Change (0-127) selects a preset the same way CC1 (preset select) does.
        no_interrupts_block([&]() {
            preset_cc_value_ = message.data1;
            preset_cc_pending_ = true;
        });
        return;
    }

    if (message.type == Midi::Type::PITCH_BEND)
    {
        // MIDI pitch bend is a 14-bit value where data1 = LSB, data2 = MSB and center = 8192.
        const uint16_t bend_value = static_cast<uint16_t>((static_cast<uint16_t>(message.data2 & 0x7FU) << 7U) |
                                                          static_cast<uint16_t>(message.data1 & 0x7FU));
        const int32_t centered_value = static_cast<int32_t>(bend_value) - 8192;
        midi_pitch_bend_semitones_ = (static_cast<float>(centered_value) / 8192.0f) * Midi::kPitchBendRangeSemitones;
        return;
    }

    if (message.type == Midi::Type::CHANNEL_AFTERTOUCH)
    {
        // Channel (global) aftertouch carries no note number, it applies to the held note whatever it is.
        HandleMidiAftertouch(message.data1);
        return;
    }

    if (message.type == Midi::Type::POLYPHONIC_AFTERTOUCH)
    {
        // Polyphonic aftertouch is per note (data1). The Badger is monophonic, so pressure for any
        // other note than the one it's playing belongs to a voice we don't have and is dropped.
        if (message.data1 == incoming_midi_note_.note)
        {
            HandleMidiAftertouch(message.data2);
        }
        return;
    }

    if (message.type == Midi::Type::CONTROL_CHANGE)
    {
        const auto layer = Badger::layers.GetCurrent();

        auto set_global_pot_cc = [&](AnalogInputs::Pot pot) {
            const uint16_t pot_value = CcToPotValue(message.data2);
            presets_.GetParams().global_pots[pot] = pot_value;

            if (layer == Layers::Layer::NORMAL)
            {
                Badger::analog_inputs.FreezePot(pot);
                global_pot_cc_active_[pot] = true;
            }
        };

        auto set_preset_pot_cc = [&](AnalogInputs::Pot pot) {
            const uint16_t pot_value = CcToPotValue(message.data2);
            presets_.GetCurrent().SetPresetPot(pot, pot_value);

            if (layer == Layers::Layer::Y)
            {
                Badger::analog_inputs.FreezePot(pot);
                preset_pot_cc_active_[pot] = true;
            }
        };

        if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::PRESET))
        {
            no_interrupts_block([&]() {
                preset_cc_value_ = message.data2;
                preset_cc_pending_ = true;
            });
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::GLOBAL_ALPHA))
        {
            set_global_pot_cc(AnalogInputs::Pot::ALPHA);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::GLOBAL_BETA))
        {
            set_global_pot_cc(AnalogInputs::Pot::BETA);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::GLOBAL_GAMMA))
        {
            set_global_pot_cc(AnalogInputs::Pot::GAMMA);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::PRESET_ALPHA))
        {
            set_preset_pot_cc(AnalogInputs::Pot::ALPHA);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::PRESET_BETA))
        {
            set_preset_pot_cc(AnalogInputs::Pot::BETA);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::PRESET_GAMMA))
        {
            set_preset_pot_cc(AnalogInputs::Pot::GAMMA);
        }

        if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::ROOT_NOTE))
        {
            const size_t root_note = TrackSequencer::CcToRootNote(message.data2);
            sequencer_.SetRootNote(root_note);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::SCALE))
        {
            const size_t scale_index = TrackSequencer::CcToScale(message.data2);
            sequencer_.SetScaleIndex(scale_index);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::OCTAVE))
        {
            const int32_t octave_offset = TrackSequencer::CcToOctaveOffset(message.data2);
            sequencer_.SetOctaveOffset(octave_offset);
        }
        else if (message.data1 == static_cast<uint8_t>(Midi::ControlChange::ALL_SOUND_OFF) ||
                 message.data1 == static_cast<uint8_t>(Midi::ControlChange::ALL_NOTES_OFF))
        {
            // MIDI panic: force off whatever note is currently held, regardless of its number.
            HandleMidiNoteOff(incoming_midi_note_.note, 0);
        }
        return;
    }

    if (message.type == Midi::Type::NOTE_ON)
    {
        // A Note On with velocity 0 is equivalent to a Note Off (common with running status).
        if (message.data2 == 0)
        {
            HandleMidiNoteOff(message.data1, message.data2);
            return;
        }

        // Switch to MIDI mode
        presets_.GetParams().play_mode = Preset::PlayMode::MIDI;
        // A note starts at its velocity outright: slewing here would fade in every attack.
        midi_velocity_gain_target_ = VelocityToGain(message.data2);
        midi_velocity_gain_current_ = midi_velocity_gain_target_;

        midi_note_held_ = true;
        incoming_midi_note_ = {true, false, message.data1, message.data2};
    }

    if (message.type == Midi::Type::NOTE_OFF)
    {
        HandleMidiNoteOff(message.data1, message.data2);
    }
}

} // namespace badger