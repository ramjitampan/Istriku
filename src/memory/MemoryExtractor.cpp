#include "MemoryExtractor.h"

#include <optional>
#include <regex>

namespace Yuki::Memory
{

std::optional<MemoryEntry>
MemoryExtractor::Extract(
    const std::string& message) const
{
    if (auto r = ExtractName(message))
        return r;

    if (auto r = ExtractCity(message))
        return r;

    if (auto r = ExtractUniversity(message))
        return r;

    if (auto r = ExtractFavorite(message))
        return r;

    return std::nullopt;
}

std::optional<MemoryEntry>
MemoryExtractor::ExtractName(
    const std::string& message) const
{
    static const std::regex pattern(
        R"(.*(?:my name is|nama saya|namaku)\s+(.+))",
        std::regex::icase);

    std::smatch match;

    if (std::regex_match(message, match, pattern))
    {
        return MemoryEntry{
            "user_name",
            match[1].str()
        };
    }

    return std::nullopt;
}

std::optional<MemoryEntry>
MemoryExtractor::ExtractCity(
    const std::string& message) const
{
    static const std::regex pattern(
        R"(.*(?:i live in|aku tinggal di|saya tinggal di)\s+(.+))",
        std::regex::icase);

    std::smatch match;

    if (std::regex_match(message, match, pattern))
    {
        return MemoryEntry{
            "user_city",
            match[1].str()
        };
    }

    return std::nullopt;
}

std::optional<MemoryEntry>
MemoryExtractor::ExtractUniversity(
    const std::string& message) const
{
    static const std::regex pattern(
        R"(.*(?:i study at|aku kuliah di|saya kuliah di)\s+(.+))",
        std::regex::icase);

    std::smatch match;

    if (std::regex_match(message, match, pattern))
    {
        return MemoryEntry{
            "university",
            match[1].str()
        };
    }

    return std::nullopt;
}

std::optional<MemoryEntry>
MemoryExtractor::ExtractFavorite(
    const std::string& message) const
{
    static const std::regex pattern(
        R"(.*(?:i like|aku suka|saya suka)\s+(.+))",
        std::regex::icase);

    std::smatch match;

    if (std::regex_match(message, match, pattern))
    {
        return MemoryEntry{
            "favorite",
            match[1].str()
        };
    }

    return std::nullopt;
}

}