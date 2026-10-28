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

#include "SaveFile.hpp"
#include "hardware/AT24C.hpp"

#include <cstdint>
#include <optional>

namespace badger
{

/**
 * @brief EEPROM persistence manager for SaveFile.
 * @details
 * The save file lives in a single record at the start of the on-board 24C02. The EEPROM is
 * good for a million write cycles and saves are throttled to one per @ref kSaveDelayMs, so
 * there is no need to rotate records around - which also leaves most of the memory free for
 * the save file to grow into.
 *
 * The record carries a checksum, so a write cut short by a power loss is recognised and
 * ignored on the next boot (the app then starts from defaults).
 *
 * A missing or broken EEPROM never gets in the way of playing: reads and writes just fail,
 * the app runs on defaults, and after @ref kMaxConsecutiveFailures failed writes the handler
 * stops touching the bus altogether.
 * @author Vaclav Mach
 * @date 2026-06-21
 */
class SaveFileHandler
{
public:
    /**
     * @brief Usable bytes of the EEPROM fitted on the board.
     */
    static constexpr uint32_t kStorageBytes = AT24C::kAT24C02Size;

    /**
     * @brief Default deferred save delay used by ScheduleSave().
     */
    static constexpr uint32_t kSaveDelayMs = 10000U;

    /**
     * @brief Failed writes in a row after which the EEPROM is given up on for good.
     */
    static constexpr uint32_t kMaxConsecutiveFailures = 3U;

    /**
     * @brief Loads the SaveFile from the EEPROM.
     * @return Loaded save file when found and valid; std::nullopt when the memory is blank,
     *         holds a damaged record, or cannot be reached at all.
     */
    std::optional<SaveFile> Load();

    /**
     * @brief Save immediately (blocking), without waiting for Process().
     * @note Meant for startup (factory reset), it takes over from a background write still in progress.
     * @return true if write succeeded.
     */
    bool SaveNow(const SaveFile &file);

    /**
     * @brief Schedule a save operation for the given SaveFile after a specified delay in milliseconds.
     * @param file The SaveFile to be saved.
     * @param delay_ms The delay in milliseconds before the save operation is executed. Default is kSaveDelayMs. Set to 0 for immediate save.
     */
    void ScheduleSave(const SaveFile &file, uint32_t delay_ms = kSaveDelayMs);

    /**
     * @brief Process any pending save operations. This should be called periodically in the UI loop.
     * @details The EEPROM is written in the background, one small step per call (see AT24C::ProcessWrite()),
     *          so a save never stalls the UI loop for the memory's write cycles.
     */
    void Process();

private:
    /**
     * @brief Wrapper around SaveFile with a checksum, as stored in the EEPROM.
     */
    struct SaveRecord
    {
        SaveFile file;
        uint32_t checksum;
    };

    /**
     * @brief Where the record sits in the EEPROM. Everything above it is free for later use.
     */
    static constexpr uint16_t kRecordAddress = 0;

    static_assert(sizeof(SaveRecord) <= kStorageBytes,
                  "SaveFile grew past what the on-board EEPROM can hold");

    static constexpr bool IsTickReached(uint32_t now, uint32_t target)
    {
        return static_cast<int32_t>(now - target) >= 0;
    }

    /**
     * @brief FNV-1a over the record's payload, which is what `checksum` holds.
     */
    static uint32_t Checksum(const SaveRecord &record);

    static bool IsValidRecord(const SaveRecord &record);

    bool WriteSaveFile(const SaveFile &file);

    /**
     * @brief Counts a failed EEPROM access and gives up on the memory once they pile up.
     */
    void NoteFailure();

    std::optional<SaveFile> pending_file_;
    std::optional<SaveFile> last_saved_file_;

    // Record being written in the background, it has to stay put until the write is over
    SaveRecord writing_record_{};
    bool writing_ = false;

    uint32_t save_due_tick_ = 0;
    uint32_t consecutive_failures_ = 0;
    bool storage_available_ = true;
};

} // namespace badger
