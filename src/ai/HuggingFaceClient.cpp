#include "HuggingFaceClient.h"

#include "../utils/Logger.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <exception>
#include <utility>

namespace Yuki::AI
{

using json = nlohmann::json;

HuggingFaceClient::HuggingFaceClient(
    const Yuki::Config::HuggingFaceConfig& config)
    : m_config(config)
{
    const std::string token = ReadTokenFromEnvironment(m_config.token_env);
    Yuki::Utils::HttpConfig httpConfig;
    httpConfig.headers["Authorization"] = "Bearer " + token;
    m_httpClient = std::make_unique<Yuki::Utils::HttpClient>(std::move(httpConfig));
    m_transport = [this](const std::string& endpoint, const std::string& body)
    {
        return m_httpClient->Post(endpoint, body);
    };
}

HuggingFaceClient::HuggingFaceClient(
    Yuki::Config::HuggingFaceConfig config,
    PostTransport transport)
    : m_config(std::move(config)),
      m_transport(std::move(transport))
{
}

std::string HuggingFaceClient::Generate(const std::string& prompt)
{
    if (prompt.empty() || m_config.endpoint.empty() ||
        m_config.model.empty() || !m_transport)
    {
        Logger::Error("[HuggingFace] Backend configuration is incomplete.");
        return {};
    }

    const auto response = m_transport(m_config.endpoint, BuildRequestBody(prompt));
    if (!response.success)
    {
        Logger::Error("[HuggingFace] Request failed with HTTP " +
                      std::to_string(response.statusCode) + ": " +
                      response.errorMessage);
        return {};
    }

    return ParseResponseText(response.body);
}

std::string HuggingFaceClient::ReadTokenFromEnvironment(
    const std::string& variableName)
{
    if (variableName.empty())
        return {};

    const char* value = std::getenv(variableName.c_str());
    return value ? value : "";
}

std::string HuggingFaceClient::BuildRequestBody(const std::string& prompt) const
{
    json body;
    body["model"] = m_config.model;
    body["messages"] = json::array({
        {{"role", "user"}, {"content", prompt}}
    });
    body["stream"] = false;
    body["max_tokens"] = m_config.max_tokens;
    return body.dump();
}

std::string HuggingFaceClient::ParseResponseText(const std::string& jsonBody) const
{
    try
    {
        const json parsed = json::parse(jsonBody);
        if (parsed.contains("choices") && parsed["choices"].is_array() &&
            !parsed["choices"].empty())
        {
            const auto& choice = parsed["choices"][0];
            if (choice.contains("message") && choice["message"].is_object() &&
                choice["message"].contains("content") &&
                choice["message"]["content"].is_string())
            {
                return choice["message"]["content"].get<std::string>();
            }
            if (choice.contains("text") && choice["text"].is_string())
                return choice["text"].get<std::string>();
        }

        // Also support the classic text-generation inference response.
        if (parsed.is_array() && !parsed.empty() &&
            parsed[0].contains("generated_text") &&
            parsed[0]["generated_text"].is_string())
        {
            return parsed[0]["generated_text"].get<std::string>();
        }
    }
    catch (const std::exception& ex)
    {
        Logger::Error(std::string("[HuggingFace] Invalid JSON response: ") + ex.what());
        return {};
    }

    Logger::Error("[HuggingFace] Response does not contain generated text.");
    return {};
}

} // namespace Yuki::AI
