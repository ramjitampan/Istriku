#include "WakeWordDetector.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Yuki::Voice
{

WakeWordDetector::WakeWordDetector(
    std::initializer_list<std::string_view> words)
{
    m_wakeWords.clear();
    for (auto& w : words)
    {
        m_wakeWords.push_back(Normalize(w));
    }
}

void WakeWordDetector::AddWakeWord(std::string_view word)
{
    auto norm = Normalize(word);
    auto it = std::find(m_wakeWords.begin(), m_wakeWords.end(), norm);
    if (it == m_wakeWords.end())
    {
        m_wakeWords.push_back(std::move(norm));
    }
}

void WakeWordDetector::RemoveWakeWord(std::string_view word)
{
    auto norm = Normalize(word);
    auto it = std::find(m_wakeWords.begin(), m_wakeWords.end(), norm);
    if (it != m_wakeWords.end())
    {
        m_wakeWords.erase(it);
    }
}

void WakeWordDetector::ClearWakeWords() noexcept
{
    m_wakeWords.clear();
}

const std::vector<std::string>&
WakeWordDetector::GetWakeWords() const noexcept
{
    return m_wakeWords;
}

std::string WakeWordDetector::Normalize(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (unsigned char c : text)
    {
        unsigned char lower = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c)));
        result += static_cast<char>(lower);
    }
    return result;
}

std::string WakeWordDetector::StripPunctuation(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (unsigned char c : text)
    {
        if (std::isalnum(c) || c == ' ' || c == '\'')
        {
            result += static_cast<char>(c);
        }
    }
    return result;
}

std::vector<std::string> WakeWordDetector::Tokenize(std::string_view text)
{
    std::vector<std::string> tokens;
    std::string current;
    for (unsigned char c : text)
    {
        if (c == ' ')
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
        }
        else
        {
            current += static_cast<char>(c);
        }
    }
    if (!current.empty())
    {
        tokens.push_back(current);
    }
    return tokens;
}

WakeWordResult WakeWordDetector::Detect(
    const std::string& text,
    std::string& commandOut) const
{
    commandOut.clear();
    if (text.empty()) return WakeWordResult::None;

    // Step 1: lowercase + strip punctuation (keep alphanum, space, apostrophe)
    std::string cleaned = StripPunctuation(Normalize(text));

    // Step 2: split into tokens
    std::vector<std::string> tokens = Tokenize(cleaned);

    if (tokens.empty()) return WakeWordResult::None;

    // Step 3: check each wake word as a multi-token sequence
    for (const auto& wake : m_wakeWords)
    {
        std::vector<std::string> wakeTokens = Tokenize(wake);

        if (wakeTokens.empty()) continue;

        // Skip if text has fewer tokens than the wake word
        if (wakeTokens.size() > tokens.size()) continue;

        // Search for the wake token sequence in the text tokens
        for (size_t i = 0; i <= tokens.size() - wakeTokens.size(); ++i)
        {
            bool match = true;
            for (size_t j = 0; j < wakeTokens.size(); ++j)
            {
                if (tokens[i + j] != wakeTokens[j])
                {
                    match = false;
                    break;
                }
            }

            if (!match) continue;

            // Wake word found at token position i
            // Extract command from tokens after the wake word
            size_t cmdTokenStart = i + wakeTokens.size();

            if (cmdTokenStart >= tokens.size())
            {
                return WakeWordResult::WakeWordOnly;
            }

            // Reconstruct command from remaining tokens
            std::string cmd;
            for (size_t k = cmdTokenStart; k < tokens.size(); ++k)
            {
                if (!cmd.empty()) cmd += ' ';
                cmd += tokens[k];
            }

            commandOut = cmd;
            return WakeWordResult::Command;
        }
    }

    return WakeWordResult::None;
}

WakeWordResult WakeWordDetector::Detect(
    const std::string& text) const
{
    std::string ignored;
    return Detect(text, ignored);
}

std::string WakeWordDetector::ToLower(std::string_view text)
{
    std::string result(text.size(), '\0');
    std::transform(
        text.begin(), text.end(), result.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
    return result;
}

} // namespace Yuki::Voice
