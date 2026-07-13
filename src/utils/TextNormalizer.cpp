#include "TextNormalizer.h"
#include "Utf8Sanitizer.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>

namespace Yuki::Utils {

std::string TextNormalizer::RemovePunctuation(std::string_view input) noexcept
{
    std::string result;
    result.reserve(input.size());
    for (unsigned char c : input)
    {
        if (std::isalnum(c) || c == ' ' || c == '\'')
        {
            result += static_cast<char>(c);
        }
    }
    return result;
}

std::string TextNormalizer::ToLower(std::string_view input) noexcept
{
    std::string result;
    result.reserve(input.size());
    for (unsigned char c : input)
    {
        result += static_cast<char>(std::tolower(c));
    }
    return result;
}

std::string TextNormalizer::Trim(std::string_view input) noexcept
{
    auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    auto last = input.find_last_not_of(" \t\r\n");
    return std::string(input.substr(first, last - first + 1));
}

std::string TextNormalizer::CollapseSpaces(std::string_view input) noexcept
{
    std::string result;
    result.reserve(input.size());
    bool lastWasSpace = false;
    for (unsigned char c : input)
    {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            if (!lastWasSpace)
            {
                result += ' ';
                lastWasSpace = true;
            }
        }
        else
        {
            result += static_cast<char>(c);
            lastWasSpace = false;
        }
    }
    return result;
}

std::string TextNormalizer::RemoveControlChars(std::string_view input) noexcept
{
    std::string result;
    result.reserve(input.size());
    for (unsigned char c : input)
    {
        if (c >= 32 || c == '\n' || c == '\t')
        {
            result += static_cast<char>(c);
        }
    }
    return result;
}

std::string TextNormalizer::Normalize(std::string_view input) noexcept
{
    std::string s = Utf8Sanitizer::Sanitize(input);
    s = ToLower(s);
    s = Trim(s);
    s = RemoveControlChars(s);
    s = CollapseSpaces(s);
    return s;
}

struct WordReplacement
{
    std::string from;
    std::string to;
};

static const std::vector<WordReplacement>& GetReplacements()
{
    // Sorted by key length descending to avoid partial matches
    static const std::vector<WordReplacement> replacements = {
        {"visual studio code", "vscode"},
        {"minimum command duration ms", "minimum_command_duration_ms"},
        {"you're key", "yuki"},
        {"youre key", "yuki"},
        {"your key", "yuki"},
        {"screen shot", "screenshot"},
        {"elima puluh", "50"},
        {"you kay", "yuki"},
        {"you key", "yuki"},
        {"youtube musik", "youtube music"},
        {"turun kan", "turunkan"},
        {"naik kan", "naikkan"},
        {"hidup kan", "hidupkan"},
        {"mati kan", "matikan"},
        {"nyala kan", "nyalakan"},
        {"buka kan", "bukakan"},
        {"kunci kan", "kuncikan"},
        {"terang kan", "terangkan"},
        {"redup kan", "redupkan"},
        {"gelap kan", "gelapkan"},
        {"aktif kan", "aktifkan"},
        {"v s code", "vscode"},
        {"vs code", "vscode"},
        {"you tube", "youtube"},
        {"wi fi", "wifi"},
        {"foto shop", "photoshop"},
        {"face book", "facebook"},
        {"u key", "yuki"},
        {"yuuki", "yuki"},
        {"yukki", "yuki"},
        {"nyenyak", "tidur"},
        {"sepuluh", "10"},
        {"sebelas", "11"},
        {"seratus", "100"},
        {"delapan", "8"},
        {"sembilan", "9"},
        {"nol", "0"},
        {"satu", "1"},
        {"dua", "2"},
        {"tiga", "3"},
        {"empat", "4"},
        {"lima", "5"},
        {"enam", "6"},
        {"tujuh", "7"},
    };
    return replacements;
}

static bool IsWordBoundary(const std::string& s, size_t pos, size_t matchLen) noexcept
{
    // Check character before match
    if (pos > 0)
    {
        unsigned char prev = static_cast<unsigned char>(s[pos - 1]);
        if (std::isalnum(prev))
            return false;
    }

    // Check character after match
    size_t after = pos + matchLen;
    if (after < s.size())
    {
        unsigned char next = static_cast<unsigned char>(s[after]);
        if (std::isalnum(next))
            return false;
    }

    return true;
}

static std::string ReplaceWholeWordOccurrences(
    std::string text,
    std::string_view from,
    std::string_view to) noexcept
{
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos)
    {
        if (!IsWordBoundary(text, pos, from.size()))
        {
            pos += 1;
            continue;
        }

        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string TextNormalizer::NormalizeRecognition(std::string_view input) noexcept
{
    std::string s = Normalize(input);

    s = RemovePunctuation(s);
    s = CollapseSpaces(s);

    const auto& reps = GetReplacements();
    for (const auto& rep : reps)
    {
        s = ReplaceWholeWordOccurrences(s, rep.from, rep.to);
    }

    s = CollapseSpaces(s);
    s = Trim(s);
    return s;
}

} // namespace Yuki::Utils
