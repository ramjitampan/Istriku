#pragma once

#include "MemoryEntry.h"
#include "MemoryManager.h"

#include <optional>
#include <string>
#include <vector>

namespace Yuki::Memory
{

// MemoryRetriever
// -----------------------------------------------------------------------
// Provides both direct key lookup (Get) and context-aware retrieval
// (GetRelevant) that inspects a user query and returns only the memory
// entries whose key is related to the query.
// -----------------------------------------------------------------------
class MemoryRetriever
{
public:
    explicit MemoryRetriever(
        MemoryManager& memoryManager);

    ~MemoryRetriever() = default;

    MemoryRetriever(const MemoryRetriever&) = delete;
    MemoryRetriever& operator=(const MemoryRetriever&) = delete;

    MemoryRetriever(MemoryRetriever&&) noexcept = default;
    MemoryRetriever& operator=(MemoryRetriever&&) noexcept = default;

    // Direct key lookup
    std::optional<std::string> Get(
        const std::string& key) const;

    // Smart retrieval: returns only entries relevant to the query.
    // Falls back to high-priority entries when no specific keyword
    // matches.
    std::vector<MemoryEntry> GetRelevant(
        const std::string& query) const;

private:
    MemoryManager& m_memoryManager;
};

}
