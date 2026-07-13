#pragma once

#include "MemoryEntry.h"

#include <string>
#include <vector>

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

    std::vector<MemoryEntry> Extract(
        const std::string& message) const;
};

}