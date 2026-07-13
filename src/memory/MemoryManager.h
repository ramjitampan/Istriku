#pragma once

#include "MemoryStorage.h"
#include "../config/Config.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Yuki::Memory
{

class MemoryManager
{
public:
    explicit MemoryManager(
        std::unique_ptr<MemoryStorage> storage);

    explicit MemoryManager(
        const Yuki::Config::MemoryConfig& config);

    ~MemoryManager() = default;

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    MemoryManager(MemoryManager&&) noexcept = default;
    MemoryManager& operator=(MemoryManager&&) noexcept = default;

    //-------------------------------------------------------
    // Save or update memory (key/value overload — backward compat)
    //-------------------------------------------------------
    bool Remember(
        const std::string& key,
        const std::string& value);

    //-------------------------------------------------------
    // Save or update memory with full entry (preserves metadata)
    //-------------------------------------------------------
    bool Remember(
        const MemoryEntry& entry);

    //-------------------------------------------------------
    // Retrieve memory
    //-------------------------------------------------------
    std::optional<std::string> Recall(
        const std::string& key);

    //-------------------------------------------------------
    // Forget one memory
    //-------------------------------------------------------
    bool Forget(
        const std::string& key);

    //-------------------------------------------------------
    // Forget everything
    //-------------------------------------------------------
    bool ForgetAll();

    //-------------------------------------------------------
    // Retrieve all memories
    //-------------------------------------------------------
    std::vector<MemoryEntry> RecallAll();

private:
    std::unique_ptr<MemoryStorage> m_storage;
};

}