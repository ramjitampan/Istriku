#pragma once

#include <string>

namespace Yuki::Chat
{

class Conversation;

class PromptBuilder
{
public:
    PromptBuilder() = default;
    ~PromptBuilder() = default;

    PromptBuilder(const PromptBuilder&) = delete;
    PromptBuilder& operator=(const PromptBuilder&) = delete;

    PromptBuilder(PromptBuilder&&) = default;
    PromptBuilder& operator=(PromptBuilder&&) = default;

    std::string BuildPrompt(
        const Conversation& conversation
    ) const;

private:
    std::string BuildHeader() const;
};

} // namespace Yuki::Chat