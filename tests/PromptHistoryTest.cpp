#include "chat/Conversation.h"
#include "chat/PromptBuilder.h"
#include "memory/MemoryStorage.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

class InMemoryStorage final : public Yuki::Memory::MemoryStorage
{
public:
    std::vector<Yuki::Memory::MemoryEntry> entries;
    int loadAllCalls = 0;

    bool Save(const Yuki::Memory::MemoryEntry& entry) override
    {
        entries.push_back(entry);
        return true;
    }

    std::optional<Yuki::Memory::MemoryEntry> Load(const std::string&) override
    {
        return std::nullopt;
    }

    bool Remove(const std::string&) override { return false; }
    bool Clear() override { entries.clear(); return true; }

    std::vector<Yuki::Memory::MemoryEntry> LoadAll() override
    {
        ++loadAllCalls;
        return entries;
    }
};

int main()
{
    int failed = 0;
    auto check = [&failed](const char* name, bool ok)
    {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << '\n';
        if (!ok) ++failed;
    };

    auto storage = std::make_unique<InMemoryStorage>();
    auto* storageView = storage.get();
    storageView->entries.emplace_back("user_name", "Ramzy");
    storageView->entries.emplace_back("user_city", "Binjai");
    Yuki::Memory::MemoryManager memories(std::move(storage));
    Yuki::Chat::PromptBuilder builder;

    Yuki::Chat::Conversation conversation;
    for (int turn = 1; turn <= 8; ++turn)
    {
        conversation.AddUserMessage("Topik giliran " + std::to_string(turn));
        if (turn < 8)
            conversation.AddAssistantMessage("Jawaban giliran " + std::to_string(turn));
    }

    const auto storedMessageCount = conversation.Size();
    const std::string bounded = builder.BuildPrompt(conversation, memories, nullptr, 6);
    check("only last six user turns are serialized",
          bounded.find("Topik giliran 1") == std::string::npos &&
          bounded.find("Topik giliran 2") == std::string::npos &&
          bounded.find("Topik giliran 3") != std::string::npos &&
          bounded.find("Topik giliran 8") != std::string::npos);
    check("recent messages retain chronological order",
          bounded.find("Topik giliran 3") < bounded.find("Topik giliran 8"));
    check("latest user message is always included", bounded.find("Topik giliran 8") != std::string::npos);
    check("stored conversation remains unchanged", conversation.Size() == storedMessageCount);
    check("memory is read once per prompt build", storageView->loadAllCalls == 1);

    Yuki::Chat::Conversation duplicateFacts;
    duplicateFacts.AddUserMessage("Namaku Ramzy dan aku tinggal di Binjai.");
    const std::string deduplicated = builder.BuildPrompt(duplicateFacts, memories);
    check("same explicit user facts are not injected twice",
          deduplicated.find("Identity\n") == std::string::npos &&
          deduplicated.find("Location\n") == std::string::npos &&
          deduplicated.find("User: Namaku Ramzy") != std::string::npos);
    check("deduplication does not delete memory entries",
          storageView->entries.size() == 2 && storageView->loadAllCalls == 2);

    Yuki::Chat::Conversation zeroLimit;
    zeroLimit.AddUserMessage("Pesan terbaru tetap ada");
    const std::string zeroLimitPrompt = builder.BuildPrompt(zeroLimit, memories, nullptr, 0);
    check("zero limit safely retains the latest user message",
          zeroLimitPrompt.find("Pesan terbaru tetap ada") != std::string::npos);

    return failed == 0 ? 0 : 1;
}
