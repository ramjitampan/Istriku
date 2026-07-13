#include "MemoryManager.h"
#include "MemoryPatterns.h"
#include "SQLiteMemoryStorage.h"

#include <regex>

namespace Yuki::Memory
{

MemoryManager::MemoryManager(
    std::unique_ptr<MemoryStorage> storage)
    : m_storage(std::move(storage))
{
}

MemoryManager::MemoryManager(
    const Yuki::Config::MemoryConfig& config)
    : m_storage(std::make_unique<SQLiteMemoryStorage>(config.database))
{
}

bool MemoryManager::Remember(
    const std::string& key,
    const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    // Defense-in-depth: skip question-word values
    auto existing = m_storage->Load(key);

    if (existing)
    {
        static const std::regex questionWord(
            R"(\b(?:apa|siapa|dimana|kapan|kenapa|bagaimana)\b)",
            std::regex::icase);

        if (std::regex_match(value, questionWord))
        {
            return true;
        }
    }

    MemoryEntry entry{key, value};
    return Remember(entry);
}

bool MemoryManager::Remember(
    const MemoryEntry& entry)
{
    if (entry.value.empty())
    {
        return false;
    }

    MemoryEntry e = entry;

    if (e.metadata.created_at == 0)
    {
        e.metadata.created_at = Now();
    }

    if (e.metadata.updated_at == 0)
    {
        e.metadata.updated_at = Now();
    }

    // Assign default priority from MemoryPatterns if not set
    if (e.metadata.priority == 0)
    {
        for (const auto& rule : GetMemoryRules())
        {
            if (rule.memoryKey == e.key)
            {
                e.metadata.priority = rule.default_priority;
                break;
            }
        }
    }

    return m_storage->Save(e);
}

std::optional<std::string>
MemoryManager::Recall(
    const std::string& key)
{
    auto result = m_storage->Load(key);

    if (!result)
    {
        return std::nullopt;
    }

    return result->value;
}

bool MemoryManager::Forget(
    const std::string& key)
{
    return m_storage->Remove(key);
}

bool MemoryManager::ForgetAll()
{
    return m_storage->Clear();
}

std::vector<MemoryEntry>
MemoryManager::RecallAll()
{
    return m_storage->LoadAll();
}

}
