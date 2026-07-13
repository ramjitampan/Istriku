#pragma once

#include "../chat/Conversation.h"
#include "../memory/MemoryManager.h"

#include <string>
#include <vector>

namespace Yuki::Chat
{

// PromptBuilder
// -----------------------------------------------------------------------
// Builds the system prompt injected into the LLM.
// Features:
//   - Formats memories into structured categories.
//   - Filters memories by relevance to the last user message
//     (smart retrieval) so the prompt stays concise.
//   - No API-breaking changes from earlier versions.
// -----------------------------------------------------------------------
class PromptBuilder
{
public:
    PromptBuilder() = default;

    std::string BuildPrompt(
        const Conversation& conversation,
        Yuki::Memory::MemoryManager& memoryManager,
        const std::string* personaPrompt = nullptr) const;

private:
    std::string BuildHeader() const;

    void AppendMemories(
        std::string& prompt,
        Yuki::Memory::MemoryManager& memoryManager,
        const std::string& lastQuery) const;

    void AppendConversation(
        std::string& prompt,
        const Conversation& conversation) const;

    static std::string FormatKey(
        const std::string& key);

    static std::string GetCategory(
        const std::string& key);

    // Smart retrieval helpers
    static std::vector<std::string> GetRelevantKeys(
        const std::string& query);

    static std::string GetLastUserMessage(
        const Conversation& conversation);
};

}
