#include "chat/PromptBuilder.h"
#include "chat/Conversation.h"

#include "memory/MemoryManager.h"
#include "memory/SQLiteMemoryStorage.h"

#include <iostream>
#include <memory>

using namespace Yuki;

int main()
{
    auto storage =
        std::make_unique<Memory::SQLiteMemoryStorage>();

    Memory::MemoryManager manager(
        std::move(storage));

    // --- Seed memories with the old API (backward compat) ---
    manager.Remember("user_name", "Ramzy");
    manager.Remember("user_city", "Binjai");
    manager.Remember("favorite", "C++");
    manager.Remember("university", "Universitas Negeri Padang");

    Chat::Conversation conversation;

    // Verify: this "Hello" query triggers no specific key,
    // so the prompt shows high-priority memories only.
    conversation.AddUserMessage("Hello");

    Chat::PromptBuilder builder;

    std::cout
        << "===== Prompt with generic greeting =====\n"
        << builder.BuildPrompt(conversation, manager)
        << "\n";

    // --- Second test: query with "tinggal" should only show user_city ---
    conversation.AddAssistantMessage("Hi! How can I help you?");
    conversation.AddUserMessage("Dimana aku tinggal?");

    std::cout
        << "===== Prompt with location query =====\n"
        << builder.BuildPrompt(conversation, manager)
        << "\n";

    // --- Third test: query with "suka" should only show favorite ---
    conversation.AddAssistantMessage("You live in Binjai.");
    conversation.AddUserMessage("Apa yang aku suka?");

    std::cout
        << "===== Prompt with preference query =====\n"
        << builder.BuildPrompt(conversation, manager)
        << "\n";

    return 0;
}
