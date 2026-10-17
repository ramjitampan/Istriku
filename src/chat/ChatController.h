#pragma once

#include "../memory/MemoryExtractor.h"
#include "../memory/MemoryManager.h"
#include "../memory/MemoryRules.h"

#include "Conversation.h"
#include "PromptBuilder.h"

#include <memory>
#include <string>
#include <vector>

namespace Yuki::AI
{
class AIBackend;
}

namespace Yuki::Config
{
struct Config;
}

namespace Yuki::Chat
{

enum class ConversationMode
{
    Perintah,
    Curhat
};

class ChatController
{
public:
    ChatController(
        std::unique_ptr<Yuki::AI::AIBackend> backend,
        Yuki::Memory::MemoryManager& memoryManager,
        const Yuki::Config::Config* config = nullptr);

    ~ChatController() = default;

    ChatController(const ChatController&) = delete;
    ChatController& operator=(const ChatController&) = delete;

    ChatController(ChatController&&) noexcept = default;
    ChatController& operator=(ChatController&&) noexcept = default;

    std::string SendMessage(
        const std::string& message);

    void ClearConversation();

    const Conversation& GetConversation() const noexcept;

    void SetMode(ConversationMode mode) noexcept;
    ConversationMode GetMode() const noexcept;

    static bool IsTriggerPhrase(
        const std::string& normalizedText,
        const std::vector<std::string>& triggerPhrases) noexcept;
    static bool IsExitPhrase(
        const std::string& normalizedText,
        const std::vector<std::string>& exitPhrases) noexcept;

private:
    std::unique_ptr<Yuki::AI::AIBackend> m_backend;

    Yuki::Memory::MemoryManager& m_memoryManager;

    Conversation m_conversation;
    PromptBuilder m_promptBuilder;

    Yuki::Memory::MemoryExtractor m_memoryExtractor;
    Yuki::Memory::MemoryRules m_memoryRules;

    const Yuki::Config::Config* m_config = nullptr;

    ConversationMode m_mode = ConversationMode::Perintah;

};

} // namespace Yuki::Chat
