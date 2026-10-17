#include "PromptBuilder.h"
#include "../config/Config.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace Yuki::Chat
{

namespace
{

// ---------------------------------------------------------------
// ToLower
// ---------------------------------------------------------------
std::string ToLower(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (unsigned char c : s)
    {
        out.push_back(static_cast<char>(std::tolower(c)));
    }

    return out;
}

// ---------------------------------------------------------------
// Relevance mapping (shared with MemoryRetriever logic)
// ---------------------------------------------------------------
struct RelevanceEntry
{
    const char* queryWord;
    const char* memoryKey;
};

const RelevanceEntry kRelevanceMap[] =
{
    {"nama",    "user_name"},
    {"namaku",  "user_name"},
    {"panggil", "user_name"},
    {"siapa",   "user_name"},

    {"tinggal", "user_city"},
    {"rumah",   "user_city"},
    {"kota",    "user_city"},
    {"dimana",  "user_city"},
    {"asal",    "user_city"},

    {"suka",    "favorite"},
    {"favorit", "favorite"},
    {"gemar",   "favorite"},
    {"hobi",    "favorite"},

    {"kuliah",       "university"},
    {"universitas",  "university"},
    {"sekolah",      "university"},
    {"jurusan",      "university"},
};

// ---------------------------------------------------------------
// Category display names
// ---------------------------------------------------------------
const char* DisplayCategory(const std::string& key)
{
    if (key == "user_name")  return "Identity";
    if (key == "user_city")  return "Location";
    if (key == "university") return "Education";
    if (key == "favorite")   return "Preferences";
    return "General";
}

} // anonymous namespace

// ===========================================================================
// BuildPrompt
// ===========================================================================

std::string PromptBuilder::BuildPrompt(
    const Conversation& conversation,
    Yuki::Memory::MemoryManager& memoryManager,
    const std::string* personaPrompt,
    std::size_t maxHistoryTurns,
    const std::string* modeInstructions) const
{
    std::string prompt;

    prompt += personaPrompt ? *personaPrompt : Yuki::Config::DefaultPersonaPrompt();
    prompt += "\n\n";
    prompt += BuildTechnicalInstructions();
    if (modeInstructions && !modeInstructions->empty())
    {
        prompt += "\n\n";
        prompt += *modeInstructions;
    }

    const auto& messages = conversation.GetMessages();
    const std::size_t historyStart = GetHistoryStart(messages, maxHistoryTurns);
    std::string lastQuery = GetLastUserMessage(conversation);
    AppendMemories(prompt, memoryManager, lastQuery, messages, historyStart);
    AppendConversation(prompt, messages, historyStart);

    prompt += "Assistant: ";

    return prompt;
}

// Technical instructions stay separate from the persona.
// ===========================================================================

std::string PromptBuilder::BuildTechnicalInstructions() const
{
    return
        "INSTRUKSI KERJA:\n"
        "Jawab semua permintaan spesifik di pesan terakhir dengan jelas dan akurat; jangan menggantinya dengan basa-basi generik. Untuk coding, pertanyaan faktual, atau hal serius, utamakan jawaban ringkas dan benar tanpa flirting, banter, atau narasi persona. Periksa inisialisasi, ownership, dan lifetime pada contoh kode; berikan kode valid dan nyatakan jika ada yang belum pasti. Gunakan hanya fakta pengguna yang diberikan atau tersedia dalam memori; jangan menebak kondisi yang tidak disebutkan, mengaku bisa melihat/mendengar pengguna, atau mengklaim punya kenangan percakapan lain yang tidak tersedia. Tulis hanya kata-kata yang diucapkan, tanpa narasi aksi atau tanda bintang. Intent Windows ditangani sistem: jangan keluarkan tag [CMD] atau mengaku aksi berhasil tanpa hasil dari sistem.";
}
// ===========================================================================
// Key / Category formatters
// ===========================================================================

std::string PromptBuilder::FormatKey(const std::string& key)
{
    if (key == "user_name")   return "Name";
    if (key == "user_city")   return "City";
    if (key == "favorite")    return "Favorite";
    if (key == "university")  return "University";
    return key;
}

std::string PromptBuilder::GetCategory(const std::string& key)
{
    return DisplayCategory(key);
}

// ===========================================================================
// Smart retrieval helpers
// ===========================================================================

std::string PromptBuilder::GetLastUserMessage(
    const Conversation& conversation)
{
    const auto& messages = conversation.GetMessages();

    for (auto it = messages.rbegin(); it != messages.rend(); ++it)
    {
        if (it->role == MessageRole::User)
        {
            return it->content;
        }
    }

    return {};
}

std::vector<std::string> PromptBuilder::GetRelevantKeys(
    const std::string& query)
{
    if (query.empty())
    {
        return {};
    }

    std::string lower = ToLower(query);
    std::vector<std::string> keys;

    for (const auto& entry : kRelevanceMap)
    {
        if (lower.find(entry.queryWord) != std::string::npos)
        {
            if (std::find(keys.begin(), keys.end(), entry.memoryKey) == keys.end())
            {
                keys.push_back(entry.memoryKey);
            }
        }
    }

    return keys;
}

// ===========================================================================
// AppendMemories — structured, relevance-filtered
// ===========================================================================

void PromptBuilder::AppendMemories(
    std::string& prompt,
    Yuki::Memory::MemoryManager& memoryManager,
    const std::string& lastQuery,
    const std::vector<Message>& messages,
    std::size_t historyStart) const
{
    auto all = memoryManager.RecallAll();

    if (all.empty())
    {
        return;
    }

    // Determine which keys are relevant to the last user message
    auto relevantKeys = GetRelevantKeys(lastQuery);

    // If no specific keywords, show the highest-priority memories
    auto filterEntry = [&](const Yuki::Memory::MemoryEntry& e) -> bool
    {
        if (relevantKeys.empty())
        {
            return e.metadata.priority >= 70;
        }

        return std::find(relevantKeys.begin(), relevantKeys.end(), e.key)
               != relevantKeys.end();
    };

    // Collect and sort by priority
    std::vector<Yuki::Memory::MemoryEntry> filtered;
    struct RecentUserContext
    {
        std::string textLower;
        std::vector<std::string> relevantKeys;
    };
    std::vector<RecentUserContext> recentUserContexts;
    recentUserContexts.reserve(messages.size() - historyStart);
    for (std::size_t i = historyStart; i < messages.size(); ++i)
    {
        if (messages[i].role == MessageRole::User)
        {
            recentUserContexts.push_back(
                {ToLower(messages[i].content), GetRelevantKeys(messages[i].content)});
        }
    }

    for (const auto& entry : all)
    {
        if (filterEntry(entry))
        {
            const std::string valueLower = ToLower(entry.value);
            bool alreadyInRecentUserContext = false;
            for (const auto& context : recentUserContexts)
            {
                if (std::find(context.relevantKeys.begin(), context.relevantKeys.end(), entry.key) ==
                    context.relevantKeys.end())
                    continue;

                if (!valueLower.empty() && context.textLower.find(valueLower) != std::string::npos)
                {
                    alreadyInRecentUserContext = true;
                    break;
                }
            }

            if (!alreadyInRecentUserContext)
                filtered.push_back(entry);
        }
    }

    if (filtered.empty())
    {
        // Fallback: show nothing (avoid bloating the prompt)
        return;
    }

    // Sort by priority descending, then by confidence descending
    std::sort(filtered.begin(), filtered.end(),
        [](const Yuki::Memory::MemoryEntry& a,
           const Yuki::Memory::MemoryEntry& b)
        {
            if (a.metadata.priority != b.metadata.priority)
                return a.metadata.priority > b.metadata.priority;
            return a.metadata.confidence > b.metadata.confidence;
        });

    // Group by category
    prompt += "These are verified user facts.\n\n";

    std::string lastCategory;

    for (const auto& entry : filtered)
    {
        std::string cat = GetCategory(entry.key);

        if (cat != lastCategory)
        {
            prompt += cat;
            prompt += "\n";
            lastCategory = cat;
        }

        prompt += "  ";
        prompt += FormatKey(entry.key);
        prompt += ": ";
        prompt += entry.value;
        prompt += "\n";
    }

    prompt += "\nThese facts are trusted.\n";
    prompt += "Use them whenever answering questions about the user.\n";
    prompt += "Never invent conflicting information.\n\n";
}

// ===========================================================================
// AppendConversation
// ===========================================================================

void PromptBuilder::AppendConversation(
    std::string& prompt,
    const std::vector<Message>& messages,
    std::size_t historyStart) const
{
    prompt += "Conversation:\n\n";

    for (std::size_t i = historyStart; i < messages.size(); ++i)
    {
        const auto& message = messages[i];
        switch (message.role)
        {
        case MessageRole::User:
            prompt += "User: ";
            break;

        case MessageRole::Assistant:
            prompt += "Assistant: ";
            break;
        }

        prompt += message.content;
        prompt += '\n';
    }

    prompt += '\n';
}

std::size_t PromptBuilder::GetHistoryStart(
    const std::vector<Message>& messages,
    std::size_t maxHistoryTurns)
{
    const std::size_t limit = std::max<std::size_t>(1, maxHistoryTurns);
    std::size_t userMessages = 0;

    for (std::size_t i = messages.size(); i > 0; --i)
    {
        if (messages[i - 1].role == MessageRole::User && ++userMessages == limit)
            return i - 1;
    }

    return 0;
}

}
