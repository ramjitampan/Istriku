#pragma once

#include "MemoryStorage.h"

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

    ~MemoryManager() = default;

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    MemoryManager(MemoryManager&&) noexcept = default;
    MemoryManager& operator=(MemoryManager&&) noexcept = default;

    //-------------------------------------------------------
    // Save or update memory
    //-------------------------------------------------------
    bool Remember(
        const std::string& key,
        const std::string& value);

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