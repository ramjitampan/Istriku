#pragma once

#include "AIBackend.h"
#include "../config/Config.h"
#include "../utils/HttpClient.h"

#include <functional>
#include <memory>
#include <string>

namespace Yuki::AI
{

class HuggingFaceClient final : public AIBackend
{
public:
    using PostTransport = std::function<Yuki::Utils::HttpResponse(
        const std::string&, const std::string&)>;

    explicit HuggingFaceClient(const Yuki::Config::HuggingFaceConfig& config);

    // Test seam for deterministic request/response verification.
    HuggingFaceClient(
        Yuki::Config::HuggingFaceConfig config,
        PostTransport transport);

    std::string Generate(const std::string& prompt) override;

    static std::string ReadTokenFromEnvironment(const std::string& variableName);

private:
    std::string BuildRequestBody(const std::string& prompt) const;
    std::string ParseResponseText(const std::string& jsonBody) const;

    Yuki::Config::HuggingFaceConfig m_config;
    std::unique_ptr<Yuki::Utils::HttpClient> m_httpClient;
    PostTransport m_transport;
};

} // namespace Yuki::AI
