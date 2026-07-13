#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace Yuki::Memory
{

// MemoryMetadata
// -----------------------------------------------------------------------
// Carries extra information about a stored fact: when it was created /
// last updated, how important it is, how confident we are, how often it
// has been mentioned, and the previous value for update history.
// -----------------------------------------------------------------------
struct MemoryMetadata
{
    std::int64_t created_at      = 0;
    std::int64_t updated_at      = 0;
    int          priority        = 0;
    float        confidence      = 1.0f;
    int          mention_count   = 0;
    std::string  previous_value;
};

// MemoryEntry
// -----------------------------------------------------------------------
// The basic unit of long-term memory.
// Two-argument constructor (key, value) is kept for backward
// compatibility — metadata gets default values.
// -----------------------------------------------------------------------
struct MemoryEntry
{
    std::string    key;
    std::string    value;
    MemoryMetadata metadata;

    MemoryEntry() = default;

    MemoryEntry(
        std::string keyValue,
        std::string valueValue)
        : key(std::move(keyValue))
        , value(std::move(valueValue))
    {
    }

    MemoryEntry(
        std::string keyValue,
        std::string valueValue,
        MemoryMetadata metaValue)
        : key(std::move(keyValue))
        , value(std::move(valueValue))
        , metadata(std::move(metaValue))
    {
    }
};

// -----------------------------------------------------------------------
// Helper: current Unix timestamp (seconds)
// -----------------------------------------------------------------------
inline std::int64_t Now()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace Yuki::Memory
