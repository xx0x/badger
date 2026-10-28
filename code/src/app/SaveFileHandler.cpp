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

#include "SaveFileHandler.hpp"

#include "Badger.hpp"
#include "py32f0xx_hal.h"

#include <cstddef>
#include <cstdint>

namespace badger
{

uint32_t SaveFileHandler::Checksum(const SaveRecord &record)
{
    // FNV-1a over everything in the record except the checksum field itself.
    static constexpr uint32_t kOffsetBasis = 0x811C9DC5U;
    static constexpr uint32_t kPrime = 0x01000193U;

    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t hash = kOffsetBasis;

    for (size_t i = 0; i < offsetof(SaveRecord, checksum); ++i)
    {
        hash ^= bytes[i];
        hash *= kPrime;
    }

    return hash;
}

bool SaveFileHandler::IsValidRecord(const SaveRecord &record)
{
    return record.file.IsValid() && (record.checksum == Checksum(record));
}

void SaveFileHandler::NoteFailure()
{
    consecutive_failures_ += 1U;

    if (consecutive_failures_ >= kMaxConsecutiveFailures)
    {
        // The memory is missing or broken. Stop touching the bus so a dead chip can never
        // stall the UI loop again, and drop whatever was waiting to be written.
        storage_available_ = false;
        pending_file_.reset();
    }
}

std::optional<SaveFile> SaveFileHandler::Load()
{
    pending_file_.reset();
    last_saved_file_.reset();

    SaveRecord record = {};

    if (!Badger::eeprom.Read(kRecordAddress, reinterpret_cast<uint8_t *>(&record), sizeof(record)))
    {
        // Nothing answered on the bus, so there is no point trying to save later either.
        storage_available_ = false;
        return std::nullopt;
    }

    if (!IsValidRecord(record))
    {
        // Blank or damaged memory. The chip itself is fine, so keep saving enabled.
        return std::nullopt;
    }

    last_saved_file_ = record.file;
    return record.file;
}

void SaveFileHandler::ScheduleSave(const SaveFile &file, uint32_t delay_ms)
{
    if (!storage_available_)
    {
        return;
    }

    if (last_saved_file_.has_value() && file == *last_saved_file_)
    {
        pending_file_.reset();
        return;
    }

    pending_file_ = file;
    save_due_tick_ = HAL_GetTick() + delay_ms;
}

bool SaveFileHandler::SaveNow(const SaveFile &file)
{
    pending_file_.reset();

    // The blocking write below takes over the memory, a background write would be abandoned by it anyway
    writing_ = false;

    if (!WriteSaveFile(file))
    {
        return false;
    }

    last_saved_file_ = file;
    return true;
}

void SaveFileHandler::Process()
{
    // Carry on with a write in progress, one small step per call
    if (writing_)
    {
        switch (Badger::eeprom.ProcessWrite())
        {
        case AT24C::WriteStatus::BUSY:
            return;

        case AT24C::WriteStatus::DONE:
            writing_ = false;
            last_saved_file_ = writing_record_.file;
            consecutive_failures_ = 0;
            return;

        case AT24C::WriteStatus::FAILED:
            writing_ = false;
            NoteFailure();

            // Retry later (unless a newer file is waiting anyway, or the failures piled up and NoteFailure() gave up)
            if (storage_available_ && !pending_file_.has_value())
            {
                pending_file_ = writing_record_.file;
            }
            save_due_tick_ = HAL_GetTick() + kSaveDelayMs;
            return;
        }
    }

    if (!pending_file_.has_value())
    {
        return;
    }

    if (!IsTickReached(HAL_GetTick(), save_due_tick_))
    {
        return;
    }

    const SaveFile file = *pending_file_;
    pending_file_.reset();

    if (!storage_available_ || (last_saved_file_.has_value() && file == *last_saved_file_))
    {
        return;
    }

    writing_record_ = {};
    writing_record_.file = file;
    writing_record_.checksum = Checksum(writing_record_);

    writing_ = Badger::eeprom.StartWrite(kRecordAddress,
                                         reinterpret_cast<const uint8_t *>(&writing_record_),
                                         sizeof(writing_record_));
}

bool SaveFileHandler::WriteSaveFile(const SaveFile &file)
{
    if (!storage_available_)
    {
        return false;
    }

    if (last_saved_file_.has_value() && file == *last_saved_file_)
    {
        return true;
    }

    SaveRecord record = {};
    record.file = file;
    record.checksum = Checksum(record);

    if (!Badger::eeprom.Write(kRecordAddress,
                              reinterpret_cast<const uint8_t *>(&record),
                              sizeof(record)))
    {
        NoteFailure();
        return false;
    }

    consecutive_failures_ = 0;
    return true;
}

} // namespace badger
