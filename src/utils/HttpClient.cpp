#include "HttpClient.h"
#include "Logger.h"

#include <memory>
#include <string>

// WinHTTP is shipped with Windows; no external dependency needed.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <stdexcept>
#include <sstream>
#include <vector>

namespace Yuki::Utils {

// ===========================================================================
// Internal helpers
// ===========================================================================
namespace {

// Converts a narrow UTF-8 string to a wide string for WinHTTP APIs.
std::wstring ToWide(const std::string& utf8)
{
    if (utf8.empty()) return {};

    const int len = MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0
    );
    if (len <= 0) return {};

    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        wide.data(), len
    );
    return wide;
}

// Converts a wide string returned by WinHTTP to narrow UTF-8.
std::string ToNarrow(const std::wstring& wide)
{
    if (wide.empty()) return {};

    const int len = WideCharToMultiByte(
        CP_UTF8, 0,
        wide.data(), static_cast<int>(wide.size()),
        nullptr, 0,
        nullptr, nullptr
    );
    if (len <= 0) return {};

    std::string narrow(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0,
        wide.data(), static_cast<int>(wide.size()),
        narrow.data(), len,
        nullptr, nullptr
    );
    return narrow;
}

// Builds a human-readable error string from a Win32 error code.
std::string Win32ErrorMessage(DWORD code)
{
    LPWSTR buf = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buf), 0, nullptr
    );

    std::string msg;
    if (buf) {
        msg = ToNarrow(buf);
        LocalFree(buf);
        // Trim trailing newline characters added by FormatMessage.
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
            msg.pop_back();
    }
    return msg.empty() ? "Unknown WinHTTP error (code=" + std::to_string(code) + ")" : msg;
}

// RAII wrappers for WinHTTP handles so we never leak on early return.
struct WinHttpHandleDeleter {
    void operator()(HINTERNET h) const noexcept {
        if (h) WinHttpCloseHandle(h);
    }
};
using WinHttpHandle = std::unique_ptr<void, WinHttpHandleDeleter>;

} // anonymous namespace

// ===========================================================================
// HttpClient::Impl  — pimpl; owns the WinHTTP session handle
// ===========================================================================
class HttpClient::Impl {
public:
    explicit Impl(HttpConfig config)
        : m_config(std::move(config))
    {
        HINTERNET raw = WinHttpOpen(
            L"Yuki/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!raw) {
            throw std::runtime_error(
                "WinHttpOpen failed: " + Win32ErrorMessage(GetLastError())
            );
        }
        m_session = WinHttpHandle(raw);

        // Apply session-level timeout (milliseconds).
        const DWORD ms = static_cast<DWORD>(m_config.timeoutMs);
        WinHttpSetTimeouts(raw, ms, ms, ms, ms);
    }

    // -----------------------------------------------------------------------
    // Core dispatch: opens a connection, sends request, reads response.
    // -----------------------------------------------------------------------
    HttpResponse Send(
        const std::string& method,
        const std::string& url,
        const std::string& body)
    {
        HttpResponse result;

        // --- Validate input early (was previously checked too late,
        //     after the URL had already been parsed/connected — dead code) --
        if (url.empty()) {
            result.errorMessage = "URL is empty.";
            Logger::Error("[HTTP] " + result.errorMessage);
            return result;
        }

        // --- Parse URL ---------------------------------------------------
        URL_COMPONENTSW urlComp{};
        urlComp.dwStructSize = sizeof(urlComp);

        wchar_t scheme[16]{}, host[256]{}, path[2048]{};
        urlComp.lpszScheme   = scheme;   urlComp.dwSchemeLength   = _countof(scheme);
        urlComp.lpszHostName = host;     urlComp.dwHostNameLength = _countof(host);
        urlComp.lpszUrlPath  = path;     urlComp.dwUrlPathLength  = _countof(path);

        const auto wUrl = ToWide(url);
        if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
            result.errorMessage = "Invalid URL: " + url;
            Logger::Error("[HTTP] " + result.errorMessage);
            return result;
        }

        const bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        const DWORD port   = (urlComp.nPort != 0)
                             ? static_cast<DWORD>(urlComp.nPort)
                             : (isHttps ? INTERNET_DEFAULT_HTTPS_PORT
                                        : INTERNET_DEFAULT_HTTP_PORT);

        // --- Connect -------------------------------------------------------
        HINTERNET rawConn = WinHttpConnect(
            m_session.get(), host, static_cast<INTERNET_PORT>(port), 0
        );
        if (!rawConn) {
            result.errorMessage = "WinHttpConnect failed: " + Win32ErrorMessage(GetLastError());
            Logger::Error("[HTTP] " + result.errorMessage);
            return result;
        }
        WinHttpHandle connection(rawConn);

        // --- Open request ----------------------------------------------------
        const DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
        const auto wMethod = ToWide(method);

        HINTERNET rawReq = WinHttpOpenRequest(
            connection.get(),
            wMethod.c_str(),
            path,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags
        );
        if (!rawReq) {
            result.errorMessage = "WinHttpOpenRequest failed: " + Win32ErrorMessage(GetLastError());
            Logger::Error("[HTTP] " + result.errorMessage);
            return result;
        }
        WinHttpHandle request(rawReq);

        // --- Add headers (config-level + per-method defaults) ----------------
        std::wstring allHeaders;

        // Content-Type for POST
        if (method == "POST")
            allHeaders += L"Content-Type: application/json\r\n";

        // User-supplied custom headers
        for (const auto& [key, val] : m_config.headers)
            allHeaders += ToWide(key + ": " + val + "\r\n");

        if (!allHeaders.empty()) {
            WinHttpAddRequestHeaders(
                request.get(),
                allHeaders.c_str(),
                static_cast<DWORD>(allHeaders.size()),
                WINHTTP_ADDREQ_FLAG_ADD
            );
        }

        // --- Send ------------------------------------------------------------
        const char* pBody   = body.empty() ? nullptr : body.data();
        const DWORD bodyLen = body.empty() ? 0 : static_cast<DWORD>(body.size());

        if (!WinHttpSendRequest(
                request.get(),
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                const_cast<char*>(pBody), bodyLen,
                bodyLen, 0))
        {
            result.errorMessage = "WinHttpSendRequest failed: " + Win32ErrorMessage(GetLastError());
            Logger::Error("[HTTP] " + result.errorMessage);
            return result;
        }

        if (!WinHttpReceiveResponse(request.get(), nullptr)) {
            result.errorMessage = "WinHttpReceiveResponse failed: " + Win32ErrorMessage(GetLastError());
            Logger::Error("[HTTP] " + result.errorMessage);
            return result;
        }

        // --- Read status code ------------------------------------------------
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(
            request.get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX
        );
        result.statusCode = static_cast<int>(statusCode);

        // --- Read body -------------------------------------------------------
        std::ostringstream bodyStream;
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request.get(), &available) && available > 0) {
            std::vector<char> buffer(available + 1, '\0');
            DWORD read = 0;
            if (WinHttpReadData(request.get(), buffer.data(), available, &read) && read > 0)
                bodyStream.write(buffer.data(), static_cast<std::streamsize>(read));
        }
        result.body    = bodyStream.str();
        result.success = (result.statusCode >= 200 && result.statusCode < 300);

        return result;
    }

    const HttpConfig& Config() const noexcept { return m_config; }

private:
    HttpConfig    m_config;
    WinHttpHandle m_session;
};

// ===========================================================================
// HttpClient  — public API
// ===========================================================================

HttpClient::HttpClient(HttpConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config)))
{}

// Destructor must be defined in the .cpp where Impl is complete.
HttpClient::~HttpClient() = default;

HttpResponse HttpClient::Get(const std::string& url)
{
    Logger::Info("[HTTP] GET " + url);
    HttpResponse resp = m_impl->Send("GET", url, {});

    if (resp.success)
        Logger::Info("[HTTP] GET " + url + " -> " + std::to_string(resp.statusCode));
    else
        Logger::Error("[HTTP] GET " + url + " -> " +
                      std::to_string(resp.statusCode) + " | " + resp.errorMessage);

    return resp;
}

HttpResponse HttpClient::Post(const std::string& url, const std::string& jsonBody)
{
    Logger::Info("[HTTP] POST -> " + url);
    HttpResponse resp = m_impl->Send("POST", url, jsonBody);

    if (resp.success)
        Logger::Info("[HTTP] POST " + url + " -> " + std::to_string(resp.statusCode));
    else
        Logger::Error("[HTTP] POST " + url + " -> " +
                      std::to_string(resp.statusCode) + " | " + resp.errorMessage);

    return resp;
}

} // namespace Yuki::Utils