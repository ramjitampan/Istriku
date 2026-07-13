#include "MemoryRules.h"

#include "MemoryPatterns.h"

#include <cctype>
#include <regex>

namespace Yuki::Memory
{

bool MemoryRules::ShouldRemember(
    const std::string& message) const
{
    if (IsQuestion(message))
    {
        return false;
    }

    // Lowercase once and check against the shared keyword list
    std::string lower;
    lower.reserve(message.size());

    for (unsigned char c : message)
    {
        lower.push_back(static_cast<char>(std::tolower(c)));
    }

    return ContainsAnyKeyword(lower);
}

bool MemoryRules::ContainsAnyKeyword(
    const std::string& lowerMsg) const
{
    for (const auto& rule : GetMemoryRules())
    {
        const std::string& kw = rule.keyword;
        size_t pos = lowerMsg.find(kw);

        if (pos == std::string::npos)
        {
            continue;
        }

        // Standalone "aku " / "saya " need a capital next word —
        // skip them here because we don't have case info.
        // They are handled conservatively: only match if the
        // keyword appears at a word boundary.
        bool leftOk = (pos == 0) ||
            !std::isalnum(static_cast<unsigned char>(lowerMsg[pos - 1]));

        if (leftOk)
        {
            return true;
        }
    }

    return false;
}

bool MemoryRules::IsQuestion(
    const std::string& message) const
{
    if (message.find('?') != std::string::npos)
    {
        return true;
    }

    static const std::regex questionWord(
        R"(\b(?:apakah|apa|siapa|kapan|bagaimana|kenapa|mengapa|berapa|ingat|remember|dimana)\b)",
        std::regex::icase);

    if (std::regex_search(message, questionWord))
    {
        return true;
    }

    static const std::regex diMana(
        R"(\bdi\s+mana\b)",
        std::regex::icase);

    return std::regex_search(message, diMana);
}

}
