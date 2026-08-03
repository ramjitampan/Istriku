#include "RemoteServer.h"

#include "../utils/Logger.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace Yuki::Remote {

namespace {

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string header;
    std::string body;
};

// Baca header sampai "\r\n\r\n", lalu baca body sesuai Content-Length.
bool ReadRequest(SOCKET client, HttpRequest& out)
{
    std::string data;
    char buf[4096];
    size_t headerEnd = std::string::npos;

    while (data.size() < 64 * 1024)
    {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0)
            break;
        data.append(buf, static_cast<size_t>(n));
        headerEnd = data.find("\r\n\r\n");
        if (headerEnd != std::string::npos)
            break;
    }

    if (headerEnd == std::string::npos)
        return false;

    out.header = data.substr(0, headerEnd);

    // Request line: "METHOD PATH HTTP/1.1"
    std::istringstream lineStream(out.header);
    lineStream >> out.method >> out.path;

    if (out.method.empty() || out.path.empty())
        return false;

    // Content-Length (case-insensitive)
    size_t contentLength = 0;
    std::string lower = out.header;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    size_t pos = lower.find("content-length:");
    if (pos != std::string::npos)
    {
        size_t vStart = pos + 15;
        while (vStart < lower.size() &&
               (lower[vStart] == ' ' || lower[vStart] == '\t'))
            ++vStart;

        size_t vEnd = vStart;
        while (vEnd < lower.size() && std::isdigit(
                   static_cast<unsigned char>(lower[vEnd])))
            ++vEnd;

        if (vEnd > vStart)
            contentLength = static_cast<size_t>(
                std::strtoul(lower.substr(vStart, vEnd - vStart).c_str(), nullptr, 10));
    }

    out.body = data.substr(headerEnd + 4);
    while (out.body.size() < contentLength)
    {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0)
            break;
        out.body.append(buf, static_cast<size_t>(n));
    }

    if (out.body.size() > contentLength)
        out.body.resize(contentLength);

    return true;
}

} // anonymous namespace

RemoteServer::RemoteServer(int port, std::string token, Handler handler)
    : m_port(port)
    , m_token(std::move(token))
    , m_handler(std::move(handler))
{
}

RemoteServer::~RemoteServer()
{
    Stop();
}

bool RemoteServer::Start()
{
    if (m_running.exchange(true))
        return true;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        Logger::Error("RemoteServer: WSAStartup failed.");
        m_running = false;
        return false;
    }

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
    {
        Logger::Error("RemoteServer: socket() failed. err=" +
                      std::to_string(WSAGetLastError()));
        m_running = false;
        return false;
    }

    int opt = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<u_short>(m_port));

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        Logger::Error("RemoteServer: bind() gagal di port " +
                      std::to_string(m_port) + ". err=" +
                      std::to_string(WSAGetLastError()));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        m_running = false;
        return false;
    }

    // Bila m_port == 0, ambil port yang sebenarnya diberikan OS.
    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    if (getsockname(m_listenSocket, reinterpret_cast<sockaddr*>(&bound), &boundLen) == 0)
        m_port = ntohs(bound.sin_port);

    if (listen(m_listenSocket, 8) == SOCKET_ERROR)
    {
        Logger::Error("RemoteServer: listen() gagal. err=" +
                      std::to_string(WSAGetLastError()));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        m_running = false;
        return false;
    }

    m_thread = std::thread(&RemoteServer::AcceptLoop, this);
    Logger::Info("RemoteServer: listening di port " + std::to_string(m_port));
    return true;
}

void RemoteServer::Stop()
{
    if (!m_running.exchange(false))
        return;

    if (m_listenSocket != INVALID_SOCKET)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    if (m_thread.joinable())
        m_thread.join();

    Logger::Info("RemoteServer: stopped.");
}

void RemoteServer::AcceptLoop()
{
    while (m_running)
    {
        sockaddr_in clientAddr{};
        int len = sizeof(clientAddr);
        SOCKET client = accept(m_listenSocket,
            reinterpret_cast<sockaddr*>(&clientAddr), &len);

        if (client == INVALID_SOCKET)
        {
            if (!m_running)
                break;
            Logger::Warning("RemoteServer: accept gagal. err=" +
                            std::to_string(WSAGetLastError()));
            continue;
        }

        std::thread(&RemoteServer::HandleConnection, this, client).detach();
    }
}

void RemoteServer::HandleConnection(SOCKET client)
{
    int timeoutMs = 10000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    HttpRequest req;
    if (!ReadRequest(client, req))
    {
        closesocket(client);
        return;
    }

    // Otorisasi opsional via header X-Yuki-Token
    if (!m_token.empty())
    {
        bool authorized =
            (req.header.find("X-Yuki-Token: " + m_token) != std::string::npos) ||
            (req.header.find("x-yuki-token: " + m_token) != std::string::npos);

        if (!authorized)
        {
            SendResponse(client, 401, "application/json",
                R"({"error":"unauthorized"})");
            closesocket(client);
            return;
        }
    }

    if (req.method == "GET" && req.path == "/api/health")
    {
        SendResponse(client, 200, "application/json",
            R"({"ok":true,"name":"Yuki"})");
    }
    else if (req.method == "POST" && req.path == "/api/say")
    {
        try
        {
            auto json = nlohmann::json::parse(req.body);
            std::string text = json.value("text", "");

            if (text.empty())
            {
                SendResponse(client, 400, "application/json",
                    R"({"error":"text required"})");
            }
            else
            {
                std::string reply = m_handler ? m_handler(text) : "";
                nlohmann::json out;
                out["reply"] = reply;
                SendResponse(client, 200, "application/json", out.dump());
            }
        }
        catch (const std::exception&)
        {
            SendResponse(client, 400, "application/json",
                R"({"error":"invalid json"})");
        }
    }
    else
    {
        SendResponse(client, 404, "application/json",
            R"({"error":"not found"})");
    }

    closesocket(client);
}

void RemoteServer::SendResponse(SOCKET client, int status,
    const std::string& contentType, const std::string& body)
{
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status << " "
       << (status == 200 ? "OK"
            : status == 401 ? "Unauthorized"
            : status == 400 ? "Bad Request"
                            : "Not Found")
       << "\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: close\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "\r\n";

    std::string head = ss.str();
    send(client, head.data(), static_cast<int>(head.size()), 0);
    if (!body.empty())
        send(client, body.data(), static_cast<int>(body.size()), 0);
}

} // namespace Yuki::Remote
