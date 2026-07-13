#include "MemoryRetriever.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace Yuki::Memory
{

namespace
{

// ---------------------------------------------------------------
// ToLower
// ---------------------------------------------------------------
std::string ToLower(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (unsigned char c : s)
    {
        out.push_back(static_cast<char>(std::tolower(c)));
    }

    return out;
}

// ---------------------------------------------------------------
// RelevanceMapping
//   Maps query keywords to the memory keys they are looking for.
//   When a query contains one of these words, only the mapped
//   memory types are returned.
// ---------------------------------------------------------------
struct RelevanceEntry
{
    const char* queryWord;
    const char* memoryKey;
};

const RelevanceEntry kRelevanceMap[] =
{
    {"nama",    "user_name"},
    {"namaku",  "user_name"},
    {"panggil", "user_name"},
    {"siapa",   "user_name"},

    {"tinggal", "user_city"},
    {"rumah",   "user_city"},
    {"kota",    "user_city"},
    {"dimana",  "user_city"},
    {"asal",    "user_city"},

    {"suka",    "favorite"},
    {"favorit", "favorite"},
    {"gemar",   "favorite"},
    {"hobi",    "favorite"},

    {"kuliah",       "university"},
    {"universitas",  "university"},
    {"sekolah",      "university"},
    {"jurusan",      "university"},
};

// ---------------------------------------------------------------
// FindRelevantKeys
//   Returns the set of memory keys that are relevant to the query.
//   Empty means "all high-priority memories".
// ---------------------------------------------------------------
std::vector<std::string> FindRelevantKeys(const std::string& lowerQuery)
{
    std::vector<std::string> keys;

    for (const auto& entry : kRelevanceMap)
    {
        if (lowerQuery.find(entry.queryWord) != std::string::npos)
        {
            // Avoid duplicates
            if (std::find(keys.begin(), keys.end(), entry.memoryKey) == keys.end())
            {
                keys.push_back(entry.memoryKey);
            }
        }
    }

    return keys;
}

} // anonymous namespace

MemoryRetriever::MemoryRetriever(
    MemoryManager& memoryManager)
    : m_memoryManager(memoryManager)
{
}

std::optional<std::string>
MemoryRetriever::Get(
    const std::string& key) const
{
    return m_memoryManager.Recall(key);
}

std::vector<MemoryEntry>
MemoryRetriever::GetRelevant(
    const std::string& query) const
{
    auto all = m_memoryManager.RecallAll();

    if (all.empty())
    {
        return {};
    }

    std::string lower = ToLower(query);
    auto relevantKeys = FindRelevantKeys(lower);

    // If no specific keywords found, return high-priority entries
    if (relevantKeys.empty())
    {
        std::vector<MemoryEntry> results;

        for (const auto& entry : all)
        {
            if (entry.metadata.priority >= 70)
            {
                results.push_back(entry);
            }
        }

        // If nothing meets the threshold, return the top 3 by priority
        if (results.empty())
        {
            std::sort(all.begin(), all.end(),
                [](const MemoryEntry& a, const MemoryEntry& b)
                {
                    return a.metadata.priority > b.metadata.priority;
                });

            size_t count = std::min<size_t>(3, all.size());
            results.assign(all.begin(), all.begin() + count);
        }

        return results;
    }

    // Filter by relevant keys
    std::vector<MemoryEntry> results;

    for (const auto& entry : all)
    {
        if (std::find(relevantKeys.begin(), relevantKeys.end(),
                      entry.key) != relevantKeys.end())
        {
            results.push_back(entry);
        }
    }

    return results;
}

}
