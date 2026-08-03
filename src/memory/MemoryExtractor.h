#pragma once

#include "MemoryEntry.h"

#include <optional>
#include <string>

namespace Yuki::Memory
{

class MemoryExtractor
{
public:
    MemoryExtractor() = default;
    ~MemoryExtractor() = default;

    MemoryExtractor(const MemoryExtractor&) = default;
    MemoryExtractor& operator=(const MemoryExtractor&) = default;

    MemoryExtractor(MemoryExtractor&&) noexcept = default;
    MemoryExtractor& operator=(MemoryExtractor&&) noexcept = default;

    // Mengekstrak SATU fakta dari kalimat jika cocok dengan pola,
    // atau std::nullopt bila tidak ada yang cocok.
    std::optional<MemoryEntry> Extract(
        const std::string& message) const;

private:
    std::optional<MemoryEntry> ExtractName(const std::string& message) const;
    std::optional<MemoryEntry> ExtractCity(const std::string& message) const;
    std::optional<MemoryEntry> ExtractUniversity(const std::string& message) const;
    std::optional<MemoryEntry> ExtractFavorite(const std::string& message) const;
};

}
