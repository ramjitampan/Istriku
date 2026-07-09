#pragma once

#include "AIBackend.h"
#include "../utils/HttpClient.h"

#include <memory>
#include <string>

namespace Yuki::AI
{

class OllamaClient : public AIBackend
{
public:

    explicit OllamaClient(
        Yuki::Utils::HttpClient& httpClient,
        std::string model = "gemma3:4b");

    std::string Generate(
        const std::string& prompt) override;

private:

    Yuki::Utils::HttpClient& m_httpClient;

    std::string m_model;

    const std::string m_endpoint =
        "http://127.0.0.1:11434/api/generate";

    std::string BuildRequestBody(
        const std::string& prompt) const;

    std::string ParseResponseText(
        const std::string& jsonBody) const;
};

}