#include "OllamaClient.h"

#include "../utils/Logger.h"

#include <nlohmann/json.hpp>
#include <exception>
#include <utility>

namespace Yuki::AI
{

using json = nlohmann::json;

OllamaClient::OllamaClient(
    Yuki::Utils::HttpClient& httpClient,
    std::string model)
    : m_httpClient(httpClient),
      m_model(std::move(model))
{
}

std::string OllamaClient::BuildRequestBody(
    const std::string& prompt) const
{
    json body;

    body["model"] = m_model;
    body["prompt"] = prompt;
    body["stream"] = false;

    return body.dump();
}

std::string OllamaClient::ParseResponseText(
    const std::string& jsonBody) const
{
    try
    {
        json parsed = json::parse(jsonBody);

        if (parsed.contains("response") &&
            parsed["response"].is_string())
        {
            return parsed["response"].get<std::string>();
        }

        Logger::Warning(
            "Ollama response does not contain a valid 'response' field.");

        return "";
    }
    catch (const std::exception& e)
    {
        Logger::Error(
            std::string("Failed to parse Ollama response: ")
            + e.what());

        return "";
    }
}

std::string OllamaClient::Generate(
    const std::string& prompt)
{
    if (prompt.empty())
    {
        Logger::Warning("Generate() called with an empty prompt.");
        return "";
    }

    auto response =
        m_httpClient.Post(
            m_endpoint,
            BuildRequestBody(prompt));

    if (!response.success || response.statusCode != 200)
    {
        Logger::Error(
            "POST /api/generate failed: " +
            response.errorMessage);

        return "";
    }

    return ParseResponseText(response.body);
}

} // namespace Yuki::AI