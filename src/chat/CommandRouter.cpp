#include "CommandRouter.h"

#include "ChatController.h"

#include "../config/Config.h"
#include "../system/IntentDetector.h"
#include "../system/WindowsController.h"
#include "../utils/Logger.h"

#include <cstdlib>

namespace Yuki::Chat
{

CommandRouter::CommandRouter(
    ChatController& chat,
    Yuki::System::WindowsController& windowsController,
    Yuki::System::IntentDetector& intentDetector,
    const Yuki::Config::Config& config)
    : m_chat(chat)
    , m_windowsController(windowsController)
    , m_intentDetector(intentDetector)
    , m_config(config)
{
}

CommandRouter::~CommandRouter() = default;

ChatController& CommandRouter::GetChat() noexcept
{
    return m_chat;
}

const ChatController& CommandRouter::GetChat() const noexcept
{
    return m_chat;
}

std::string CommandRouter::HandleText(const std::string& text)
{
    if (text.empty())
        return {};

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_chat.GetMode() == ConversationMode::Curhat)
        return HandleCurhatMode(text);

    return HandlePerintahMode(text);
}

// ============================================================
// Mode CURHAT — command dikenal tetap dieksekusi, sisanya ke AI
// ============================================================

std::string CommandRouter::HandleCurhatMode(const std::string& text)
{
    // Cek exit phrase dulu
    if (ChatController::IsExitPhrase(
            text, m_config.voice.curhat_exit_phrases))
    {
        Logger::Info("[INFO] Mode changed: Curhat -> Perintah (exit: \"" +
                     text + "\")");
        m_chat.SetMode(ConversationMode::Perintah);

        // JANGAN kirim ke AI, cukup balasan lokal
        const char* replies[] = {
            "Oke, aku balik siaga ya~",
            "Baik sayang, kalau butuh aku lagi, panggil aja.",
            "Siap, kembali ke mode perintah. Aku di sini kok."
        };
        int idx = (std::rand() / 32) % 3;
        return replies[idx];
    }

    // Command yang dikenal tetap dieksekusi langsung walau lagi curhat
    Yuki::System::Command command = m_intentDetector.Detect(text);
    if (command.type != Yuki::System::CommandType::Unknown)
    {
        Logger::Info("CommandRouter: Intent (curhat mode): " +
                     std::string(Yuki::System::CommandTypeToString(command.type)));

        if (command.type == Yuki::System::CommandType::Brightness ||
            command.type == Yuki::System::CommandType::Volume)
        {
            return m_chat.SendMessage(text);
        }

        Yuki::System::CommandResult result =
            m_windowsController.Execute(command);

        if (result.confirmationRequired)
            return m_chat.SendMessage(text);

        return result.message;
    }

    // Bukan command — kirim ke AI dengan persona
    Logger::Info("CommandRouter: Sending to AI (curhat mode)...");
    return m_chat.SendMessage(text);
}

// ============================================================
// Mode PERINTAH (default) — command dijalankan, sisanya ke AI
// ============================================================

std::string CommandRouter::HandlePerintahMode(const std::string& text)
{
    // Command desktop (pre-AI routing)
    Yuki::System::Command command = m_intentDetector.Detect(text);

    if (command.type != Yuki::System::CommandType::Unknown)
    {
        Logger::Info("CommandRouter: Intent: " +
                     std::string(Yuki::System::CommandTypeToString(command.type)));

        if (command.type == Yuki::System::CommandType::Brightness ||
            command.type == Yuki::System::CommandType::Volume)
        {
            // Biarkan ChatController menangani logika brightness/volume
            return m_chat.SendMessage(text);
        }

        Yuki::System::CommandResult result =
            m_windowsController.Execute(command);

        if (result.confirmationRequired)
            return m_chat.SendMessage(text);

        return result.message;
    }

    // Bukan intent — cek trigger phrase untuk masuk mode curhat
    if (ChatController::IsTriggerPhrase(
            text, m_config.voice.curhat_trigger_phrases))
    {
        Logger::Info("[INFO] Mode changed: Perintah -> Curhat (trigger: \"" +
                     text + "\")");
        m_chat.SetMode(ConversationMode::Curhat);

        // Kirim teks ini ke AI dengan persona
        Logger::Info("CommandRouter: Sending to AI (curhat trigger)...");
        return m_chat.SendMessage(text);
    }

    // Bukan command, bukan trigger — kirim ke AI (bukan fallback lokal).
    Logger::Info("CommandRouter: No intent — sending to AI");
    return m_chat.SendMessage(text);
}

} // namespace Yuki::Chat
