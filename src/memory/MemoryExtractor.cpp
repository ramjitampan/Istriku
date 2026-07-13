#include "MemoryExtractor.h"

#include "MemoryPatterns.h"

#include <algorithm>
#include <cctype>

namespace Yuki::Memory
{

// ===========================================================================
// Internal helpers
// ===========================================================================

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
// Trim
// ---------------------------------------------------------------
std::string Trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };

    auto front = std::find_if(s.begin(), s.end(), notSpace);
    s.erase(s.begin(), front);

    auto back = std::find_if(s.rbegin(), s.rend(), notSpace);
    s.erase(back.base(), s.end());

    return s;
}

// ---------------------------------------------------------------
// IsStandaloneNameKeyword
// ---------------------------------------------------------------
bool IsStandaloneNameKeyword(const std::string& keyword)
{
    return keyword == "aku " || keyword == "saya ";
}

// ---------------------------------------------------------------
// Match — represents a single keyword occurrence in the message
// ---------------------------------------------------------------
struct Match
{
    size_t       pos;    // start index in the lowercased message
    size_t       end;    // one past the last matched character
    const MemoryRule* rule;
};

// ---------------------------------------------------------------
// FindAllMatches
//   Scans the lowercased message for every keyword that appears
//   at a word boundary (preceded by start-of-string or a
//   non-alphanumeric character).
// ---------------------------------------------------------------
std::vector<Match> FindAllMatches(
    const std::string& lowerMsg,
    const std::vector<MemoryRule>& rules)
{
    std::vector<Match> matches;

    for (const auto& rule : rules)
    {
        const std::string& kw = rule.keyword;
        size_t pos = 0;

        while (true)
        {
            pos = lowerMsg.find(kw, pos);

            if (pos == std::string::npos)
            {
                break;
            }

            // Word-boundary check on the left
            bool leftOk = (pos == 0) ||
                !std::isalnum(static_cast<unsigned char>(lowerMsg[pos - 1]));

            if (leftOk)
            {
                matches.push_back({pos, pos + kw.size(), &rule});
            }

            pos += 1;
        }
    }

    return matches;
}

// ---------------------------------------------------------------
// ResolveOverlaps
//   When two matches overlap, the longer one wins.  If equal
//   length, the earlier one wins.  Assumes input is sorted by
//   position.
// ---------------------------------------------------------------
std::vector<Match> ResolveOverlaps(std::vector<Match> matches)
{
    if (matches.empty())
    {
        return {};
    }

    // Sort by (position, -length) so earlier + longer comes first
    std::sort(matches.begin(), matches.end(),
        [](const Match& a, const Match& b)
        {
            if (a.pos != b.pos) return a.pos < b.pos;
            return a.end > b.end;  // longer match first
        });

    std::vector<Match> clean;
    clean.reserve(matches.size());

    for (const auto& m : matches)
    {
        if (clean.empty())
        {
            clean.push_back(m);
            continue;
        }

        Match& last = clean.back();

        // If m starts inside the last match, skip it (last is longer
        // or equal by virtue of the sort).
        if (m.pos < last.end)
        {
            continue;
        }

        clean.push_back(m);
    }

    return clean;
}

// ---------------------------------------------------------------
// ValidateMatch
//   Extra constraints beyond boundary + overlap resolution.
//   Currently used only for standalone "aku " / "saya " to
//   require the next word to start with an uppercase letter
//   (avoids false positives like "aku berlari").
// ---------------------------------------------------------------
bool ValidateMatch(
    const Match& m,
    const std::string& original)
{
    if (!IsStandaloneNameKeyword(m.rule->keyword))
    {
        return true;
    }

    // Peek past the keyword for the first non-space character
    // in the ORIGINAL (case-preserving) message.
    size_t cursor = m.end;

    while (cursor < original.size() && std::isspace(
        static_cast<unsigned char>(original[cursor])))
    {
        cursor++;
    }

    if (cursor >= original.size())
    {
        return false;
    }

    unsigned char first = static_cast<unsigned char>(original[cursor]);
    return std::isupper(first) != 0;
}

// ---------------------------------------------------------------
// AnalyzeConfidence
//   Returns a confidence score in [0,1] based on hedging language
//   in the original message.
// ---------------------------------------------------------------
float AnalyzeConfidence(const std::string& original)
{
    std::string lower = ToLower(original);

    // Strong hedging phrases → moderate confidence
    static const char* hedgePatterns[] = {
        "aku rasa", "kurasa", "aku kira", "sepertinya",
        "mungkin", "barangkali", "rasanya",
        "i think", "i guess", "maybe", "probably",
        "not sure", "i believe"
    };

    for (const char* pattern : hedgePatterns)
    {
        if (lower.find(pattern) != std::string::npos)
        {
            return 0.55f;
        }
    }

    return 1.0f;
}

// ---------------------------------------------------------------
// ExtractValues
//   For each match, extract the substring between its end and the
//   next match's start, trim it, and build a MemoryEntry with
//   metadata (confidence, priority).
// ---------------------------------------------------------------
std::vector<MemoryEntry> ExtractValues(
    const std::vector<Match>& matches,
    const std::string& original,
    float confidence)
{
    std::vector<MemoryEntry> results;

    for (size_t i = 0; i < matches.size(); ++i)
    {
        const auto& m = matches[i];

        size_t valueStart = m.end;
        size_t valueEnd = (i + 1 < matches.size())
            ? matches[i + 1].pos
            : original.size();

        std::string value = Trim(
            original.substr(valueStart, valueEnd - valueStart));

        if (value.empty())
        {
            continue;
        }

        MemoryEntry entry{m.rule->memoryKey, value};

        entry.metadata.confidence = confidence;
        entry.metadata.priority   = m.rule->default_priority;

        results.push_back(std::move(entry));
    }

    return results;
}

} // anonymous namespace

// ===========================================================================
// MemoryExtractor::Extract
// ===========================================================================

std::vector<MemoryEntry>
MemoryExtractor::Extract(
    const std::string& message) const
{
    // 1. Lowercase the message for pattern matching
    std::string lower = ToLower(message);

    // 2. Find every keyword occurrence
    auto matches = FindAllMatches(lower, GetMemoryRules());

    if (matches.empty())
    {
        return {};
    }

    // 3. Remove overlapping matches (longest keyword wins)
    auto clean = ResolveOverlaps(std::move(matches));

    // 4. Validate each match (e.g. standalone aku/saya needs capital)
    std::vector<Match> validated;
    validated.reserve(clean.size());

    for (const auto& m : clean)
    {
        if (ValidateMatch(m, message))
        {
            validated.push_back(m);
        }
    }

    // 5. Analyze confidence from hedging words
    float confidence = AnalyzeConfidence(message);

    // 6. Extract values between consecutive matches with metadata
    return ExtractValues(validated, message, confidence);
}

} // namespace Yuki::Memory
