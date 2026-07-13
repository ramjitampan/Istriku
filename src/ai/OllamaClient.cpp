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
      m_model(std::move(model)),
      m_endpoint("http://127.0.0.1:11434/api/generate")
{
}

OllamaClient::OllamaClient(
    Yuki::Utils::HttpClient& httpClient,
    const Yuki::Config::OllamaConfig& config)
    : m_httpClient(httpClient),
      m_model(config.model),
      m_endpoint(config.host + "/api/generate")
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
            std::string raw = parsed["response"].get<std::string>();

            // Filter karakter: retain hanya ASCII + Latin-1 Supplement (U+0000-U+00FF)
            // Pakai iterasi codepoint UTF-8 supaya TIDAK memotong di tengah multi-byte
            std::string clean;
            clean.reserve(raw.size());

            size_t i = 0;
            while (i < raw.size())
            {
                unsigned char c = static_cast<unsigned char>(raw[i]);

                if (c < 0x80)
                {
                    // ASCII — retain semua
                    clean.push_back(raw[i]);
                    i += 1;
                }
                else if (c >= 0xC0)
                {
                    // Determine sequence length from leading byte
                    unsigned char len = 1;
                    if ((c & 0xE0) == 0xC0)       len = 2;
                    else if ((c & 0xF0) == 0xE0)  len = 3;
                    else if ((c & 0xF8) == 0xF0)  len = 4;

                    if (len == 1 || i + len > raw.size())
                    {
                        i += 1;
                        continue;
                    }

                    if (len == 2)
                    {
                        // 2-byte: U+0080-U+07FF — retain hanya Latin-1 (U+0080-U+00FF)
                        unsigned char b2 = static_cast<unsigned char>(raw[i + 1]);
                        if ((b2 & 0xC0) == 0x80)
                        {
                            uint32_t cp = ((c & 0x1F) << 6) | (b2 & 0x3F);
                            if (cp <= 0xFF)
                            {
                                clean.push_back(raw[i]);
                                clean.push_back(raw[i + 1]);
                            }
                        }
                        i += 2;
                    }
                    else
                    {
                        // 3/4-byte = non-Latin (emoji, CJK, dll) — skip seluruh sequence
                        i += len;
                    }
                }
                else
                {
                    // Continuation byte stray — skip
                    i += 1;
                }
            }

            return clean;
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