#include "CommandRouter.h"

#include "ChatController.h"
#include "../config/Config.h"
#include "../system/IntentDetector.h"
#include "../system/WindowsController.h"
#include "../utils/Logger.h"

#ifdef SendMessage
#undef SendMessage
#endif

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>

namespace Yuki::Chat
{
namespace
{
std::string Normalize(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    std::string value = text.substr(first, last - first + 1);
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool IsOneOf(const std::string& value, std::initializer_list<const char*> words)
{
    return std::any_of(words.begin(), words.end(), [&](const char* word)
    {
        return value == word;
    });
}
}

CommandRouter::CommandRouter(
    ChatController& chat,
    Yuki::System::WindowsController& windowsController,
    Yuki::System::IntentDetector& intentDetector,
    const Yuki::Config::Config& config)
    : m_chat(chat)
    , m_windowsController(windowsController)
    , m_intentDetector(intentDetector)
{
    (void)config;
}

CommandRouter::~CommandRouter() = default;

ChatController& CommandRouter::GetChat() noexcept { return m_chat; }
const ChatController& CommandRouter::GetChat() const noexcept { return m_chat; }

ConversationMode CommandRouter::GetMode() const noexcept
{
    return m_chat.GetMode();
}

std::string CommandRouter::HandleText(const std::string& text)
{
    const std::string normalized = Normalize(text);
    if (normalized.empty()) return {};

    std::lock_guard<std::mutex> lock(m_mutex);

    if (normalized == "/perintah")
    {
        m_pendingCommand.reset();
        m_chat.SetMode(ConversationMode::Perintah);
        Logger::Info("[MODE] Perintah aktif");
        return "Mode Perintah aktif. Hanya perintah yang dikenali yang akan dijalankan.";
    }
    if (normalized == "/curhat")
    {
        m_pendingCommand.reset();
        m_chat.SetMode(ConversationMode::Curhat);
        Logger::Info("[MODE] Curhat aktif");
        return "Mode Curhat aktif. Kamu bisa mengobrol atau bertanya dengan bebas.";
    }
    if (normalized == "/mode")
    {
        return m_chat.GetMode() == ConversationMode::Curhat
            ? "Mode aktif: Curhat."
            : "Mode aktif: Perintah.";
    }

    if (m_chat.GetMode() == ConversationMode::Curhat)
        return HandleCurhatMode(text);
    return HandlePerintahMode(text);
}

std::string CommandRouter::HandleCurhatMode(const std::string& text)
{
    // Chat text never reaches IntentDetector or WindowsController in Curhat mode.
    Logger::Info("CommandRouter: chat message to AI (Curhat mode)");
    return (m_chat.SendMessage)(text);
}

std::string CommandRouter::HandlePerintahMode(const std::string& text)
{
    if (m_pendingCommand.has_value())
        return HandlePendingConfirmation(text);

    const Yuki::System::Command command = m_intentDetector.Detect(text);
    if (command.type == Yuki::System::CommandType::Unknown)
    {
        Logger::Info("CommandRouter: unknown command; AI was not called");
        return "Perintah tidak dikenali. Gunakan /curhat untuk mengobrol.";
    }

    Logger::Info("CommandRouter: intent " +
        std::string(Yuki::System::CommandTypeToString(command.type)));
    return ExecuteCommand(command);
}

std::string CommandRouter::ExecuteCommand(Yuki::System::Command command)
{
    if (command.type == Yuki::System::CommandType::Brightness)
    {
        if (command.parameter < 0)
        {
            const int current = m_windowsController.GetCurrentBrightnessPercent();
            if (current < 0)
                return "Nilai kecerahan saat ini tidak bisa dibaca. Sebutkan tingkat kecerahan 0 sampai 100 persen.";
            command.parameter = command.parameter == -3
                ? std::max(0, current - 10)
                : std::min(100, current + 10);
        }
    }
    else if (command.type == Yuki::System::CommandType::Volume)
    {
        if (command.parameter == -1 || command.parameter == -2)
        {
            int current = m_windowsController.GetCurrentVolumePercent();
            if (current < 0) current = 50;
            command.parameter = command.parameter == -1
                ? std::min(100, current + 10)
                : std::max(0, current - 10);
        }
    }

    Yuki::System::CommandResult result = m_windowsController.Execute(command);
    if (result.confirmationRequired)
    {
        m_pendingCommand = command;
        return result.message;
    }
    return result.message;
}

std::string CommandRouter::HandlePendingConfirmation(const std::string& text)
{
    const std::string answer = Normalize(text);
    if (IsOneOf(answer, {"ya", "iya", "yakin", "lanjutkan", "yes", "y",
                          "oke", "ok", "setuju", "benar", "tentu", "gas"}))
    {
        Yuki::System::Command command = *m_pendingCommand;
        command.requiresConfirmation = false;
        m_pendingCommand.reset();
        return m_windowsController.Execute(command).message;
    }
    if (IsOneOf(answer, {"tidak", "nggak", "kaga", "gak", "batalkan", "batal",
                          "jangan", "no", "n", "cancel", "enggak", "ga jadi"}))
    {
        m_pendingCommand.reset();
        return "Perintah dibatalkan.";
    }

    m_pendingCommand.reset();
    return HandlePerintahMode(text);
}

} // namespace Yuki::Chat
