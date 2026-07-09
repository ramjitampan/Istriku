#include "../src/ai/OllamaClient.h"

#include <iostream>

int main()
{
    Yuki::Utils::HttpClient http;

    Yuki::AI::OllamaClient ai(http);

    auto text = ai.Generate("Hello");

    std::cout << text << std::endl;

    return 0;
}