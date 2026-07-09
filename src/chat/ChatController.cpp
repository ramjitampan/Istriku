#include "ChatController.h"

#include "../ai/AIBackend.h"

namespace Yuki::Chat
{

ChatController::ChatController(
    std::unique_ptr<Yuki::AI::AIBackend> backend)
    : m_backend(std::move(backend))
{
}

std::string ChatController::SendMessage(
    const std::string& message)
{
    if (!m_backend)
    {
        return "Error: AI backend is not initialized.";
    }

    // Simpan pesan user
    m_conversation.AddUserMessage(message);

    // Bangun prompt dari seluruh percakapan
    std::string prompt = m_promptBuilder.BuildPrompt(m_conversation);

    // Kirim ke AI
    std::string reply = m_backend->Generate(prompt);

    // Simpan jawaban AI
    m_conversation.AddAssistantMessage(reply);

    return reply;
}

void ChatController::ClearConversation()
{
    m_conversation.Clear();
}

const Conversation& ChatController::GetConversation() const noexcept
{
    return m_conversation;
}

} // namespace Yuki::Chat