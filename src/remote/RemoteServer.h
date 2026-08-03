#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>

namespace Yuki::Remote {

// RemoteServer
// -----------------------------------------------------------------------
// HTTP server minimal (Winsock) supaya HP bisa ngobrol/perintah ke Yuki
// lewat jaringan lokal (LAN). Tiap request dieksekusi lewat Handler yang
// biasanya membungkus CommandRouter::HandleText.
// -----------------------------------------------------------------------
class RemoteServer
{
public:
    using Handler = std::function<std::string(const std::string& text)>;

    RemoteServer(int port, std::string token, Handler handler);
    ~RemoteServer();

    RemoteServer(const RemoteServer&) = delete;
    RemoteServer& operator=(const RemoteServer&) = delete;
    RemoteServer(RemoteServer&&) noexcept = delete;
    RemoteServer& operator=(RemoteServer&&) noexcept = delete;

    bool Start();
    void Stop();
    bool IsRunning() const noexcept { return m_running.load(); }
    int GetPort() const noexcept { return m_port; }

private:
    void AcceptLoop();
    void HandleConnection(SOCKET client);

    void SendResponse(SOCKET client, int status,
        const std::string& contentType, const std::string& body);

    int m_port;
    std::string m_token;
    Handler m_handler;

    SOCKET m_listenSocket = INVALID_SOCKET;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace Yuki::Remote
