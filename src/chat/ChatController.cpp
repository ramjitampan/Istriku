#include "ChatController.h"
#include "../ai/AIBackend.h"
#include "../system/Command.h"
#include "../utils/StringFuzzy.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>

namespace Yuki::Chat {

ChatController::ChatController(
    std::unique_ptr<Yuki::AI::AIBackend> backend,
    Yuki::Memory::MemoryManager& memoryManager,
    const Yuki::Config::Config* config)
    : m_backend(std::move(backend))
    , m_memoryManager(memoryManager)
    , m_config(config)
{
}

static std::string ParseCommandFromResponse(
    const std::string& response,
    Yuki::System::Command& commandOut)
{
    std::regex cmdRegex(R"(\[CMD:(\w+)\s*(\d*)\])");
    std::smatch match;

    if (std::regex_search(response, match, cmdRegex))
    {
        std::string cmdName = match[1].str();
        std::string paramStr = match[2].str();
        int param = paramStr.empty() ? -1 : std::stoi(paramStr);

        // Map string to CommandType
        auto& ct = commandOut.type;
        if (cmdName == "OpenVSCode") ct = Yuki::System::CommandType::OpenVSCode;
        else if (cmdName == "OpenSteam") ct = Yuki::System::CommandType::OpenSteam;
        else if (cmdName == "OpenExplorer") ct = Yuki::System::CommandType::OpenExplorer;
        else if (cmdName == "OpenYoutubeMusic") ct = Yuki::System::CommandType::OpenYoutubeMusic;
        else if (cmdName == "OpenGithub") ct = Yuki::System::CommandType::OpenGithub;
        else if (cmdName == "Screenshot") ct = Yuki::System::CommandType::Screenshot;
        else if (cmdName == "VolumeUp") ct = Yuki::System::CommandType::Volume;
        else if (cmdName == "VolumeDown") { ct = Yuki::System::CommandType::Volume; param = -2; }
        else if (cmdName == "Mute") { ct = Yuki::System::CommandType::Volume; param = 0; }
        else if (cmdName == "BrightnessUp") { ct = Yuki::System::CommandType::Brightness; param = -2; }
        else if (cmdName == "BrightnessDown") { ct = Yuki::System::CommandType::Brightness; param = -3; }
        else if (cmdName == "Brightness") ct = Yuki::System::CommandType::Brightness;
        else if (cmdName == "Shutdown") ct = Yuki::System::CommandType::Shutdown;
        else if (cmdName == "Restart") ct = Yuki::System::CommandType::Restart;
        else if (cmdName == "Sleep") ct = Yuki::System::CommandType::Sleep;
        else if (cmdName == "Lock") ct = Yuki::System::CommandType::Lock;
        else if (cmdName == "Logout") ct = Yuki::System::CommandType::Logout;
        else if (cmdName == "WifiOn") ct = Yuki::System::CommandType::WifiOn;
        else if (cmdName == "WifiOff") ct = Yuki::System::CommandType::WifiOff;

        if (ct != Yuki::System::CommandType::Unknown)
        {
            commandOut.parameter = param;
            if (ct == Yuki::System::CommandType::Shutdown ||
                ct == Yuki::System::CommandType::Restart ||
                ct == Yuki::System::CommandType::Sleep)
                commandOut.requiresConfirmation = true;

            // Return response without the [CMD:...] tag
            return std::regex_replace(response, cmdRegex, "");
        }
    }

    return response;
}

std::string ChatController::SendMessage(const std::string& message)
{
    if (!m_backend)
        return "Error: AI backend is not initialized.";

    // Handle pending confirmation
    if (m_pendingCommand.has_value())
    {
        std::string lower;
        lower.reserve(message.size());
        for (char c : message) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

        static const char* confirmWords[] = {"ya", "iya", "yakin", "lanjutkan", "yes", "y", "oke", "ok", "setuju", "benar", "tentu", "gas"};
        static const char* rejectWords[] = {"tidak", "nggak", "kaga", "gak", "batalkan", "batal", "jangan", "no", "n", "cancel", "enggak"};

        bool confirmed = false;
        for (auto* w : confirmWords)
        {
            if (lower.find(w) != std::string::npos) { confirmed = true; break; }
        }

        if (confirmed)
        {
            Yuki::System::Command cmd = *m_pendingCommand;
            cmd.requiresConfirmation = false;
            m_pendingCommand.reset();
            Yuki::System::CommandResult result = m_windowsController.Execute(cmd);
            return result.message;
        }

        bool rejected = false;
        for (auto* w : rejectWords)
        {
            if (lower.find(w) != std::string::npos) { rejected = true; break; }
        }

        if (rejected)
        {
            m_pendingCommand.reset();
            return "Oke sayang, dibatalin.";
        }

        // Not confirm or reject — clear pending, proceed as normal
        m_pendingCommand.reset();
    }

    // Step 1: Detect desktop command via rule-based IntentDetector
    Yuki::System::Command command = m_intentDetector.Detect(message);

    if (command.type != Yuki::System::CommandType::Unknown)
    {
        return HandleDesktopCommand(command);
    }

    // Step 2: Send to AI — Gemma bisa ngeinterpretasi teks yang kacau
    m_conversation.AddUserMessage(message);

    if (m_memoryRules.ShouldRemember(message))
    {
        auto memories = m_memoryExtractor.Extract(message);
        for (const auto& memory : memories)
            m_memoryManager.Remember(memory);
    }

    std::string prompt;
    if (m_mode == ConversationMode::Curhat && m_config)
    {
        prompt = m_promptBuilder.BuildPrompt(
            m_conversation, m_memoryManager, &m_config->persona_prompt);
    }
    else
    {
        prompt = m_promptBuilder.BuildPrompt(m_conversation, m_memoryManager);
    }
    std::string reply = m_backend->Generate(prompt);

    if (reply.empty())
    {
        // AI skip (noise/gak jelas) — gak usah ditambah ke conversation
        m_conversation.RemoveLastUserMessage();
        return {};
    }

    // Step 3: Parse [CMD:...] dari response AI
    Yuki::System::Command aiCommand;
    std::string cleanReply = ParseCommandFromResponse(reply, aiCommand);

    if (aiCommand.type != Yuki::System::CommandType::Unknown)
    {
        m_conversation.AddAssistantMessage(cleanReply);
        std::string execReply = HandleDesktopCommand(aiCommand);
        return cleanReply + " " + execReply;
    }

    m_conversation.AddAssistantMessage(reply);
    return reply;
}

void ChatController::ClearConversation()
{
    m_conversation.Clear();
}

const Conversation& ChatController::GetConversation() const noexcept
{
    return m_conversation;
}

std::string ChatController::HandleDesktopCommand(const Yuki::System::Command& command)
{
    if (command.type == Yuki::System::CommandType::Brightness)
    {
        return HandleBrightnessCommand(command.parameter);
    }

    if (command.type == Yuki::System::CommandType::Volume)
    {
        return HandleVolumeCommand(command.parameter);
    }

    Yuki::System::CommandResult result = m_windowsController.Execute(command);

    if (result.confirmationRequired)
    {
        m_pendingCommand = command;
        return result.message;
    }

    if (!result.success)
    {
        return result.message;
    }

    return result.message;
}

std::string ChatController::HandleBrightnessCommand(int parameter)
{
    // Negative parameters mean relative adjustment
    if (parameter == -1)
    {
        return "Apakah maksudmu 100%?";
    }

    if (parameter == -2)
    {
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Brightness;
        cmd.parameter = 15;
        auto result = m_windowsController.Execute(cmd);
        return result.message;
    }

    if (parameter == -3)
    {
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Brightness;
        cmd.parameter = 15;
        auto result = m_windowsController.Execute(cmd);
        return result.message;
    }

    Yuki::System::Command cmd;
    cmd.type = Yuki::System::CommandType::Brightness;
    cmd.parameter = parameter;
    auto result = m_windowsController.Execute(cmd);
    return result.message;
}

std::string ChatController::HandleVolumeCommand(int parameter)
{
    if (parameter == -1)
    {
        int currentPercent = m_windowsController.GetCurrentVolumePercent();
        if (currentPercent < 0) currentPercent = 50;
        int newVol = std::min(100, currentPercent + 10);
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Volume;
        cmd.parameter = newVol;
        auto result = m_windowsController.Execute(cmd);
        return result.message;
    }

    if (parameter == -2)
    {
        int currentPercent = m_windowsController.GetCurrentVolumePercent();
        if (currentPercent < 0) currentPercent = 50;
        int newVol = std::max(0, currentPercent - 10);
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Volume;
        cmd.parameter = newVol;
        auto result = m_windowsController.Execute(cmd);
        return result.message;
    }

    Yuki::System::Command cmd;
    cmd.type = Yuki::System::CommandType::Volume;
    cmd.parameter = parameter;
    auto result = m_windowsController.Execute(cmd);
    return result.message;
}

void ChatController::SetMode(ConversationMode mode) noexcept
{
    m_mode = mode;
}

ConversationMode ChatController::GetMode() const noexcept
{
    return m_mode;
}

static size_t FuzzyThreshold(size_t wordLen) noexcept
{
    if (wordLen < 6) return 1;
    if (wordLen <= 8) return 2;
    return 3;
}

static void TokenizeWords(
    const std::string& text,
    std::vector<std::string>& outWords) noexcept
{
    outWords.clear();
    std::istringstream stream(text);
    std::string word;
    while (stream >> word)
        outWords.push_back(word);
}

static void ExtractKeyWords(
    const std::vector<std::string>& phrases,
    std::vector<std::string>& outKeys) noexcept
{
    outKeys.clear();
    for (const auto& phrase : phrases)
    {
        std::istringstream stream(phrase);
        std::string word;
        while (stream >> word)
        {
            if (word.size() >= 4)
                outKeys.push_back(word);
        }
    }
}

static bool FuzzyMatchAny(
    const std::vector<std::string>& inputWords,
    const std::vector<std::string>& keyWords) noexcept
{
    for (const auto& iw : inputWords)
    {
        size_t threshold = FuzzyThreshold(iw.size());
        for (const auto& kw : keyWords)
        {
            if (Yuki::Utils::LevenshteinDistance(iw, kw) <= threshold)
                return true;
        }
    }
    return false;
}

bool ChatController::IsTriggerPhrase(
    const std::string& normalizedText,
    const std::vector<std::string>& triggerPhrases) noexcept
{
    // Fast path: exact substring match
    for (const auto& phrase : triggerPhrases)
    {
        if (normalizedText.find(phrase) != std::string::npos)
            return true;
    }

    // Fuzzy fallback: key words from trigger phrases
    std::vector<std::string> keyWords;
    ExtractKeyWords(triggerPhrases, keyWords);
    if (keyWords.empty()) return false;

    std::vector<std::string> inputWords;
    TokenizeWords(normalizedText, inputWords);
    if (inputWords.empty()) return false;

    return FuzzyMatchAny(inputWords, keyWords);
}

bool ChatController::IsExitPhrase(
    const std::string& normalizedText,
    const std::vector<std::string>& exitPhrases) noexcept
{
    // Fast path: exact substring match
    for (const auto& phrase : exitPhrases)
    {
        if (normalizedText.find(phrase) != std::string::npos)
            return true;
    }

    // Fuzzy fallback
    std::vector<std::string> keyWords;
    ExtractKeyWords(exitPhrases, keyWords);
    if (keyWords.empty()) return false;

    std::vector<std::string> inputWords;
    TokenizeWords(normalizedText, inputWords);
    if (inputWords.empty()) return false;

    return FuzzyMatchAny(inputWords, keyWords);
}

} // namespace Yuki::Chat
