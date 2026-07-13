#pragma once

#include <string>

namespace Yuki::Memory
{

// MemoryRules
// -----------------------------------------------------------------------
// Decides whether a chat message contains a fact worth remembering.
//
// Question detection (IsQuestion) uses word-boundary search for
// interrogatives anywhere in the message.
//
// Pattern matching (ShouldRemember) delegates to the shared
// MemoryPatterns list — no duplicate pattern definitions.
// -----------------------------------------------------------------------
class MemoryRules
{
public:
    MemoryRules() = default;
    ~MemoryRules() = default;

    MemoryRules(const MemoryRules&) = default;
    MemoryRules& operator=(const MemoryRules&) = default;

    MemoryRules(MemoryRules&&) noexcept = default;
    MemoryRules& operator=(MemoryRules&&) noexcept = default;

    // True only when the message is a statement (not a question) that
    // contains at least one known memory keyword.
    bool ShouldRemember(
        const std::string& message) const;

    // True when the message reads as a question (contains '?',
    // an interrogative word, or "ingat"/"remember").
    bool IsQuestion(
        const std::string& message) const;

private:
    bool ContainsAnyKeyword(
        const std::string& lowerMsg) const;
};

}
