#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace Yuki::Utils {

// ---------------------------------------------------------------------------
// HttpResponse
// Plain data structure returned by every HTTP operation.
// ---------------------------------------------------------------------------
struct HttpResponse {
    int         statusCode  = 0;
    std::string body;
    bool        success     = false;
    std::string errorMessage;
};

// ---------------------------------------------------------------------------
// HttpConfig
// Optional per-request or per-client settings.
// ---------------------------------------------------------------------------
struct HttpConfig {
    int                                        timeoutMs     = 120000;
    std::unordered_map<std::string, std::string> headers;
};

// ---------------------------------------------------------------------------
// HttpClient
//
// Responsibility: HTTP communication only.
// Does not know about AI, Ollama, prompts, memory, or any domain concept.
//
// Usage:
//   HttpClient client;
//   auto resp = client.Get("http://example.com/api/data");
//   auto resp = client.Post("http://example.com/api", R"({"key":"value"})");
//
// All POST requests set Content-Type: application/json automatically.
// Custom headers can be supplied via HttpConfig.
// ---------------------------------------------------------------------------
class HttpClient {
public:
    explicit HttpClient(HttpConfig config = {});
    ~HttpClient();

    // Non-copyable; moving is allowed.
    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&)                 = default;
    HttpClient& operator=(HttpClient&&)      = default;

    HttpResponse Get (const std::string& url);
    HttpResponse Post(const std::string& url, const std::string& jsonBody);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Yuki::Utils