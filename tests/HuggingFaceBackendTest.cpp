#include "ai/HuggingFaceClient.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

namespace
{
int g_failed = 0;

void Test(const std::string& name, bool condition)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    if (!condition)
        ++g_failed;
}
}

int main()
{
    Yuki::Config::HuggingFaceConfig config;
    config.model = "ramzy/Yukihana";
    config.endpoint = "https://example.test/v1/chat/completions";
    config.max_tokens = 321;

    std::string capturedEndpoint;
    std::string capturedBody;
    Yuki::AI::HuggingFaceClient client(config,
        [&](const std::string& endpoint, const std::string& body)
        {
            capturedEndpoint = endpoint;
            capturedBody = body;
            return Yuki::Utils::HttpResponse{
                200,
                R"({"choices":[{"message":{"content":"Halo, sayang."}}]})",
                true,
                ""};
        });

    Test("parses chat completion response",
         client.Generate("Tes prompt") == "Halo, sayang.");
    Test("uses configured endpoint", capturedEndpoint == config.endpoint);

    const auto request = nlohmann::json::parse(capturedBody);
    Test("sends configured model", request["model"] == config.model);
    Test("sends prompt as user message",
         request["messages"][0]["role"] == "user" &&
         request["messages"][0]["content"] == "Tes prompt");
    Test("sends max token limit", request["max_tokens"] == 321);
    Test("disables streaming", request["stream"] == false);

    Yuki::AI::HuggingFaceClient classicClient(config,
        [](const std::string&, const std::string&)
        {
            return Yuki::Utils::HttpResponse{
                200, R"([{"generated_text":"Classic response"}])", true, ""};
        });
    Test("parses classic inference response",
         classicClient.Generate("Tes") == "Classic response");

    Yuki::AI::HuggingFaceClient failedClient(config,
        [](const std::string&, const std::string&)
        {
            return Yuki::Utils::HttpResponse{401, "", false, "unauthorized"};
        });
    Test("returns empty string on HTTP failure", failedClient.Generate("Tes").empty());

    Yuki::AI::HuggingFaceClient malformedClient(config,
        [](const std::string&, const std::string&)
        {
            return Yuki::Utils::HttpResponse{200, "not json", true, ""};
        });
    Test("returns empty string for malformed JSON",
         malformedClient.Generate("Tes").empty());

    std::cout << "Failures: " << g_failed << '\n';
    return g_failed == 0 ? 0 : 1;
}
