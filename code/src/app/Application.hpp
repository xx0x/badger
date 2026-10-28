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

#include "Presets.hpp"
#include "app/SaveFileHandler.hpp"
#include "common/Clock.hpp"
#include "common/Color.hpp"
#include "common/Midi.hpp"
#include "common/MidiNoteSender.hpp"
#include "dsp/control/AdsrEnv.hpp"
#include "dsp/effects/SimpleHardClipper.hpp"
#include "dsp/filters/Svf.hpp"
#include "dsp/math/qmath.hpp"
#include "dsp/sampling/EightBitSamplePlayer.hpp"
#include "dsp/sequencing/TrackSequencer.hpp"
#include "dsp/synthesis/KarplusStrong.hpp"
#include "dsp/synthesis/LoFiFm.hpp"
#include "dsp/synthesis/OscillatorQ15.hpp"
#include "hardware/AudioInterface.hpp"
#include "hardware/Layers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace badger
{

#define VERSION_PLAYS_BADGER true

/**
 * @brief Main Badger application handling UI, sequencing, audio, and MIDI.
 * @author Vaclav Mach
 * @date 2026-06-04
 */
class Application
{
public:
    /**
     * @brief Initializes subsystems, restores persisted state, and prepares runtime defaults.
     * @return Always returns true for now.
     */
    bool Init();

    /**
     * @brief Executes one UI/control iteration (buttons, pots, LEDs, randomize, preset changes).
     */
    void UiLoop();

    /**
     * @brief Renders one audio callback block.
     * @param output_buffer Destination stereo PCM frames for this callback.
     */
    void AudioLoop(AudioInterface::Buffer output_buffer);

private:
    /**
     * @brief The MIDI ports that carry notes, control changes, and transport.
     */
    using MidiPort = MidiClockSource::Port;

    /**
     * @brief Buffered MIDI realtime transport events crossing loop boundaries.
     */
    struct InOutMidi
    {
        volatile size_t clock_pulses = 0;
        volatile bool start = false;
        volatile bool stop = false;
        volatile bool cont = false;
    };

    /**
     * @brief Active transport/tempo source (internal, MIDI, or external).
     */
    Clock clock_;

    /**
     * @brief Shared sequencer producing rhythm triggers and melody notes.
     */
    TrackSequencer sequencer_;

    /**
     * @brief Preset bank with shared DSP and shared preset parameters.
     */
    Presets presets_;

    /**
     * @brief Master output gain in q15 domain.
     */
    q15_t volume_ = SaveFile::kDefaultVolume;

    /**
     * @brief Output gain the velocity/aftertouch slew is heading for. Q15_MAX (no attenuation) outside of MIDI play mode.
     * @details Written from the MIDI receive path: a Note On jumps this and the applied gain together to the
     *          note's velocity, aftertouch moves only this one and leaves the audio loop to catch up.
     */
    q15_t midi_velocity_gain_target_ = Q15_MAX;

    /**
     * @brief Output gain actually applied to the audio, slewed one step per frame towards midi_velocity_gain_target_.
     */
    q15_t midi_velocity_gain_current_ = Q15_MAX;

    /**
     * @brief True while an incoming MIDI note is held, i.e. between its Note On and its Note Off.
     * @details Gates aftertouch, which must not change the gain of an already released note.
     */
    bool midi_note_held_ = false;

    /**
     * @brief Pitch offset in semitones from the last incoming MIDI pitch bend message. Combined with the
     *        CV input's v/oct reading into Preset::Params::pitch_bend on every UI loop by UpdatePitchBend().
     */
    float midi_pitch_bend_semitones_ = 0.0f;

    /**
     * @brief How many presets to jump through while being in scroll mode.
     */
    int32_t scroll_presets_ = 0;

    /**
     * @brief Set from the Audio Loop. True if scroll should be processed in the next UI loop. (We can't jump in the audio loop.)
     */
    bool scroll_presets_flag_ = false;

    /**
     * @brief Set when a preset-select CC value has been received and should be processed in UiLoop().
     */
    volatile bool preset_cc_pending_ = false;

    /**
     * @brief Last received raw preset-select CC value.
     */
    volatile uint8_t preset_cc_value_ = 0;

    /**
     * @brief Active flags for global virtual-pot overrides.
     */
    EnumArray<AnalogInputs::Pot, bool> global_pot_cc_active_ = {false, false, false};

    /**
     * @brief Active flags for preset-local virtual-pot overrides.
     */
    EnumArray<AnalogInputs::Pot, bool> preset_pot_cc_active_ = {false, false, false};

    /**
     * @brief Incoming MIDI realtime events, kept per port so the clock can tell them apart.
     *  */
    EnumArray<MidiPort, InOutMidi> incoming_midi_;

    /**
     * @brief Outgoing MIDI realtime events, emitted on every port.
     *  */
    InOutMidi outgoing_midi_;

    /**
     * @brief One-shot incoming note event delivered to presets each UI cycle.
     */
    Preset::MidiNote incoming_midi_note_;

    /**
     * @brief Shared primary-track outgoing MIDI note scheduler/sender.
     */
    MidiNoteSender midi_note_sender_;

    /**
     * @brief EEPROM save scheduler/handler.
     */
    SaveFileHandler save_file_handler_;

    /**
     * @brief Persisted save file mirrored from the EEPROM.
     */
    SaveFile save_file_ = {};

    /**
     * @brief Set when save_file_ has unsaved modifications.
     */
    bool save_file_dirty_ = false;

    /**
     * @brief Consumes the incoming MIDI queues and updates transport/note state.
     * @return Clock pulse inputs collected from the USB and TRS MIDI ports.
     */
    ClockSource::Inputs ProcessIncomingMidi();

    /**
     * @brief Builds outgoing MIDI realtime flags from current clock processing result.
     */
    void PrepareOutgoingMidi(const ClockSource::ProcessResult &clock_result);

    /**
     * @brief Emits pending MIDI realtime messages collected in outgoing_midi_ on every port.
     * @note Separate from PrepareOutgoingMidi() to avoid sending MIDI in the audio callback (which would crash the audio).
     */
    void SendOutgoingMidi();

    /**
     * @brief Sets the device MIDI channel on every port. The channel is a single global setting.
     * @param channel Zero-indexed MIDI channel (0-15) or `Midi::OMNI_CHANNEL`.
     */
    void SetMidiChannel(uint8_t channel);

    /**
     * @brief Loads persisted settings and preset pots from the EEPROM.
     * @note Falls back to the SaveFile defaults when the memory is blank, damaged or missing.
     */
    void LoadSavedSettings();

    /**
     * @brief Pushes save_file_ into the runtime state it describes.
     * @details Both startup paths go through this, so the SaveFile member defaults are what a
     *          factory-reset device actually comes up with - otherwise the defaults would only
     *          reach the hardware on the following boot, once they had been read back.
     */
    void ApplySaveFile();

    /**
     * @brief Restores default configuration and writes it to the EEPROM.
     */
    void FactoryReset();

    /**
     * @brief Randomizes sequencer/pots according to current preset randomization policy.
     */
    void Randomize();

    /**
     * @brief Samples and normalizes light sensors into shared preset params.
     */
    void LoadLightSensors();

    /**
     * @brief Combines the last MIDI pitch bend with the current CV input reading (v/oct) into shared preset params.
     */
    void UpdatePitchBend();

    /**
     * @brief Stores current preset local pots into persistence buffer.
     */
    void SavePresetPots();

    /**
     * @brief Handles scrolling the presets.
     */
    void ScrollPresets();

    /**
     * @brief Handles deferred processing of incoming preset-select CC messages.
     */
    void ProcessPendingPresetCc();

    /**
     * @brief Clears active preset-local CC overrides.
     */
    void ClearPresetPotCcOverrides();

    /**
     * @brief Maps MIDI CC value (0-127) to internal pot value (0-4095).
     */
    static constexpr uint16_t CcToPotValue(uint8_t cc_value)
    {
        return static_cast<uint16_t>((static_cast<uint32_t>(cc_value) * AnalogInputs::kMaxValue) / 127U);
    }

    /**
     * @brief Velocity (0-127) to output gain curve. Soft touches stay audible instead of fading to silence.
     */
    static constexpr auto kVelocityGainMap = MapDef<q15_t, 3>{
        {0, 64, 127},
        {q15(0.25f), q15(0.7f), q15(1.0f)}};

    /**
     * @brief Maps a raw MIDI note-on velocity (0-127) to a q15 output gain.
     */
    static constexpr q15_t VelocityToGain(uint8_t velocity)
    {
        return curve_map(static_cast<q15_t>(velocity), kVelocityGainMap, MapClamp::TRUE, MapSafe::FALSE);
    }

    /**
     * @brief Time a full-scale (silence to unity) velocity gain change is spread over, in milliseconds.
     * @details Aftertouch arrives as coarse steps, and dropping one straight into the gain is a
     *          discontinuity in the output that clicks. Ramping removes the click; the full 200 ms only
     *          applies to a silence-to-unity jump, the small steps of a pressure gesture settle proportionally faster.
     */
    static constexpr float kVelocityGainSlewMs = 200.0f;

    /**
     * @brief Per-frame increment of the velocity gain slew, derived from kVelocityGainSlewMs.
     */
    static constexpr q15_t kVelocityGainSlewStep = static_cast<q15_t>(
        static_cast<float>(Q15_MAX) /
        (kVelocityGainSlewMs * 0.001f * static_cast<float>(AudioInterface::kSampleRateHz)));

    static_assert(kVelocityGainSlewStep >= 1, "Velocity gain slew step must advance, or the slew never reaches its target");

    /**
     * @brief Advances the applied velocity gain one frame towards its target, called per audio frame.
     */
    void SlewMidiVelocityGain()
    {
        // Sampled once: the MIDI receive path can move the target out from under us mid-slew.
        const q15_t target = midi_velocity_gain_target_;
        const q15_t delta = target - midi_velocity_gain_current_;

        if (delta > kVelocityGainSlewStep)
        {
            midi_velocity_gain_current_ += kVelocityGainSlewStep;
        }
        else if (delta < -kVelocityGainSlewStep)
        {
            midi_velocity_gain_current_ -= kVelocityGainSlewStep;
        }
        else
        {
            // Within one step of the target, land exactly on it instead of dithering around it.
            midi_velocity_gain_current_ = target;
        }
    }

    /**
     * @brief Releases the currently tracked incoming MIDI note, if it matches, and clears its velocity gain.
     * @details Shared by regular Note Off, velocity-0 Note On (equivalent to Note Off per the MIDI spec),
     *          and MIDI panic (All Sound Off / All Notes Off CC messages).
     */
    void HandleMidiNoteOff(uint8_t note, uint8_t release_velocity);

    /**
     * @brief Applies an aftertouch pressure value (0-127) to the held note's output gain.
     * @details Shared by channel (global) and polyphonic aftertouch, the latter only once the message's
     *          note number has been matched against the note being played. Ignored while no note is held.
     * @param pressure Raw MIDI aftertouch pressure.
     */
    void HandleMidiAftertouch(uint8_t pressure);

    /**
     *  @brief Handles MIDI realtime transport message (clock/start/stop/continue).
     *  @param port Port the message arrived on.
     *  @param message The realtime message.
     */
    void HandleMidiRealTimeMessage(MidiPort port, const Midi::Message &message);

    /**
     * @brief Handles channel voice MIDI message (note on/off and related control).
     */
    void HandleMidiChannelMessage(const Midi::Message &message);

    /**
     * @brief Returns true if the application is in regular operation mode (not in version announcement mode).
     */
    bool RegularOperation() const;

#if VERSION_PLAYS_BADGER
    /**
     * @brief Version announcement sample player.
     */
    EightBitSamplePlayer version_sample_player_;
#endif

    /**
     * @brief Last time the version announcement blinked.
     */
    uint32_t version_last_time_blinked_ = 0;

    /**
     * @brief Current blink state of the version announcement.
     */
    bool version_blink_state_ = false;

    /**
     * @brief How many times the version announcement has blinked.
     */
    size_t version_blink_count_ = 0;
};

} // namespace badger