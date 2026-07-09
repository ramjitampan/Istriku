#pragma once

#include <string>

namespace Yuki::Memory
{
struct MemoryEntry
{
    std::string key;
    std::string value;

    MemoryEntry() = default;

    MemoryEntry(
        std::string keyValue,
        std::string valueValue)
        :
        key(std::move(keyValue)),
        value(std::move(valueValue))
    {
    }
};

} // namespace Yuki::Memory