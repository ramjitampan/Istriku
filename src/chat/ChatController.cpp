#include "ChatController.h"
#include "../ai/AIBackend.h"
#include "../utils/Logger.h"
#include "../utils/StringFuzzy.h"

#include <chrono>
#include <regex>
#include <sstream>

namespace Yuki::Chat {

namespace {

const std::string kCurhatModeInstructions =
    "MODE CURHAT: dengarkan dulu dan tanggapi dengan empati serta gaya santai. "
    "Tetap bantu secara konkret bila pengguna meminta solusi.";

class SendMessagePerfLog
{
public:
    SendMessagePerfLog()
        : m_start(std::chrono::steady_clock::now())
    {
    }

    ~SendMessagePerfLog()
    {
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - m_start).count();
        Logger::Info("[PERF] ChatController::SendMessage total_duration_us=" +
                     std::to_string(elapsedUs));
    }

private:
    std::chrono::steady_clock::time_point m_start;
};

} // namespace

ChatController::ChatController(
    std::unique_ptr<Yuki::AI::AIBackend> backend,
    Yuki::Memory::MemoryManager& memoryManager,
    const Yuki::Config::Config* config)
    : m_backend(std::move(backend))
    , m_memoryManager(memoryManager)
    , m_config(config)
{
}

static std::string StripCommandTags(const std::string& response)
{
    // AI murni ngobrol — buang sisa tag [CMD:...] yang mungkin terhalusinasi
    // model biar gak tampil mentah. Tag ini TIDAK pernah dieksekusi.
    std::string cleaned = std::regex_replace(
        response,
        std::regex(R"(\[CMD:\w+\s*\d*\])"),
        "");

    size_t start = cleaned.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};

    size_t end = cleaned.find_last_not_of(" \t\r\n");
    return cleaned.substr(start, end - start + 1);
}

std::string ChatController::SendMessage(const std::string& message)
{
    const SendMessagePerfLog perfLog;

    if (m_mode != ConversationMode::Curhat)
        return "Mode Perintah aktif. Gunakan /curhat untuk mengobrol dengan Yuki.";
    if (!m_backend)
        return "Error: AI backend is not initialized.";

    m_conversation.AddUserMessage(message);

    if (m_memoryRules.ShouldRemember(message))
    {
        auto memory = m_memoryExtractor.Extract(message);
        if (memory)
            m_memoryManager.Remember(*memory);
    }

    std::string prompt;
    const auto promptBuildStart = std::chrono::steady_clock::now();
    const std::string* personaPrompt = m_config ? &m_config->persona_prompt : nullptr;
    const std::size_t maxHistoryTurns =
        m_config ? m_config->conversation.max_history_turns : 6;
    prompt = m_promptBuilder.BuildPrompt(
        m_conversation, m_memoryManager, personaPrompt,
        maxHistoryTurns, &kCurhatModeInstructions);
    const auto promptBuildUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - promptBuildStart).count();
    Logger::Info("[PERF] prompt_build_duration_us=" +
                 std::to_string(promptBuildUs) +
                 " prompt_size_bytes=" + std::to_string(prompt.size()));

    std::string reply = m_backend->Generate(prompt);
    if (reply.empty())
    {
        m_conversation.RemoveLastUserMessage();
        return {};
    }

    std::string cleanReply = StripCommandTags(reply);
    m_conversation.AddAssistantMessage(cleanReply);
    return cleanReply;
}
void ChatController::ClearConversation()
{
    m_conversation.Clear();
}

const Conversation& ChatController::GetConversation() const noexcept
{
    return m_conversation;
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
