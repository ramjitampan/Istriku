#pragma once

#include <memory>
#include <string>

#include "Conversation.h"
#include "PromptBuilder.h"

namespace Yuki::AI
{
class AIBackend;
}

namespace Yuki::Chat
{

class ChatController
{
public:
    explicit ChatController(
        std::unique_ptr<Yuki::AI::AIBackend> backend);

    ~ChatController() = default;

    ChatController(const ChatController&) = delete;
    ChatController& operator=(const ChatController&) = delete;

    ChatController(ChatController&&) noexcept = default;
    ChatController& operator=(ChatController&&) noexcept = default;

    std::string SendMessage(const std::string& message);

    void ClearConversation();

    const Conversation& GetConversation() const noexcept;

private:
    // AI dimiliki ChatController
    std::unique_ptr<Yuki::AI::AIBackend> m_backend;

    // Riwayat chat
    Conversation m_conversation;

    // Penyusun prompt
    PromptBuilder m_promptBuilder;
};

} // namespace Yuki::Chat