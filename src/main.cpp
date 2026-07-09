#include "chat/ChatController.h"
#include "ai/OllamaClient.h"
#include "utils/HttpClient.h"

#include <iostream>
#include <memory>

using namespace Yuki;

int main()
{
    Utils::HttpClient http;

    auto backend =
        std::make_unique<AI::OllamaClient>(http);

    Chat::ChatController chat(
        std::move(backend));

    std::cout
    << chat.SendMessage("What did I say before?")
    << '\n';
}