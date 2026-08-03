#pragma once

#include "../system/Command.h"

#include <mutex>
#include <string>

namespace Yuki::Config { struct Config; }
namespace Yuki::Chat { class ChatController; }
namespace Yuki::System { class WindowsController; class IntentDetector; }

namespace Yuki::Chat
{

// ============================================================
// CommandRouter
//
// Memisahkan dua jalur pemrosesan teks:
//   1. Command desktop (buka app, shutdown, volume, dll) -> WindowsController
//   2. Percakapan AI (curhat / chat) -> ChatController -> Ollama
//
// Teks yang tidak cocok dengan command apa pun dikirim ke AI,
// bukan dibalas dengan fallback lokal. Thread-safe (pakai mutex),
// jadi aman dipanggil dari thread UI (tray) maupun thread server.
// ============================================================

class CommandRouter
{
public:
    CommandRouter(
        ChatController& chat,
        Yuki::System::WindowsController& windowsController,
        Yuki::System::IntentDetector& intentDetector,
        const Yuki::Config::Config& config);

    ~CommandRouter();

    CommandRouter(const CommandRouter&) = delete;
    CommandRouter& operator=(const CommandRouter&) = delete;

    // Memproses teks dan mengembalikan balasan (untuk ditampilkan).
    std::string HandleText(const std::string& text);

    ChatController& GetChat() noexcept;
    const ChatController& GetChat() const noexcept;

private:
    std::string HandleCurhatMode(const std::string& text);
    std::string HandlePerintahMode(const std::string& text);

    ChatController& m_chat;
    Yuki::System::WindowsController& m_windowsController;
    Yuki::System::IntentDetector& m_intentDetector;
    const Yuki::Config::Config& m_config;

    std::mutex m_mutex;
};

} // namespace Yuki::Chat
