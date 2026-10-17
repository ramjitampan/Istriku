#pragma once

#include "../system/Command.h"

#include <mutex>
#include <optional>
#include <string>

namespace Yuki::Config { struct Config; }
namespace Yuki::Chat { class ChatController; }
namespace Yuki::Chat { enum class ConversationMode; }
namespace Yuki::System { class WindowsController; class IntentDetector; }

namespace Yuki::Chat
{

// ============================================================
// CommandRouter
//
// Routes explicit Curhat chat and recognized Windows commands separately.
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
    ConversationMode GetMode() const noexcept;

    ChatController& GetChat() noexcept;
    const ChatController& GetChat() const noexcept;

private:
    std::string HandleCurhatMode(const std::string& text);
    std::string HandlePerintahMode(const std::string& text);
    std::string ExecuteCommand(Yuki::System::Command command);
    std::string HandlePendingConfirmation(const std::string& text);

    ChatController& m_chat;
    Yuki::System::WindowsController& m_windowsController;
    Yuki::System::IntentDetector& m_intentDetector;

    std::optional<Yuki::System::Command> m_pendingCommand;
    std::mutex m_mutex;
};

} // namespace Yuki::Chat
