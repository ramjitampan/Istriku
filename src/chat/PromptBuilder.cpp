#include "PromptBuilder.h"

#include "Conversation.h"

#include <sstream>

namespace Yuki::Chat
{

std::string PromptBuilder::BuildPrompt(
    const Conversation& conversation) const
{
    std::ostringstream stream;

    stream << BuildHeader();

    const auto& messages = conversation.GetMessages();

    for (const auto& message : messages)
    {
        switch (message.role)
        {
        case MessageRole::User:
            stream << "User: ";
            break;

        case MessageRole::Assistant:
            stream << "Assistant: ";
            break;
        }

        stream << message.content << '\n';
    }

    stream << "Assistant: ";

    return stream.str();
}

std::string PromptBuilder::BuildHeader() const
{
    return
        "You are Yuki, an AI desktop assistant.\n"
        "Continue the conversation naturally.\n\n";
}

} // namespace Yuki::Chat