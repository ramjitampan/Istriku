#include "MemoryManager.h"

namespace Yuki::Memory
{

MemoryManager::MemoryManager(
    std::unique_ptr<MemoryStorage> storage)
    : m_storage(std::move(storage))
{
}

bool MemoryManager::Remember(
    const std::string& key,
    const std::string& value)
{
    MemoryEntry entry;

    entry.key = key;
    entry.value = value;

    return m_storage->Save(entry);
}

std::optional<std::string>
MemoryManager::Recall(
    const std::string& key)
{
    auto result =
        m_storage->Load(key);

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