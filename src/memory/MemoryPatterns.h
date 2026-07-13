#pragma once

#include <string>
#include <vector>

namespace Yuki::Memory
{

// MemoryRule
// -----------------------------------------------------------------------
// Maps a trigger keyword to a memory storage key.
// Each rule also carries a display category and a default priority so
// that the system can group / rank facts without hard-coded switches.
// -----------------------------------------------------------------------
struct MemoryRule
{
    std::string keyword;           // lowercase trigger, e.g. "aku suka "
    std::string memoryKey;         // storage key, e.g. "favorite"
    std::string category;          // display grouping, e.g. "Identity"
    int         default_priority;  // 0-100, higher = more important
};

// GetMemoryRules
// -----------------------------------------------------------------------
// Returns the canonical list of memory extraction rules.
// Every consumer (MemoryRules, MemoryExtractor, MemoryRetriever,
// PromptBuilder) reads from this one list.
//
// Adding a new memory type:
//   1. Append a MemoryRule{ "trigger ", "key", "Category", priority }.
//   2. Optionally add a relevance mapping in MemoryRetriever if the
//      new key should be returned by smart queries.
// -----------------------------------------------------------------------
inline const std::vector<MemoryRule>& GetMemoryRules()
{
    static const std::vector<MemoryRule> rules =
    {
        // =============================================================
        // Identity (highest priority)
        // =============================================================
        {"nama saya ",           "user_name",    "Identity",   100},
        {"namaku ",              "user_name",    "Identity",   100},
        {"my name is ",          "user_name",    "Identity",   100},
        {"aku ",                 "user_name",    "Identity",   100},
        {"saya ",                "user_name",    "Identity",   100},

        // =============================================================
        // Location
        // =============================================================
        {"aku berasal dari ",    "user_city",    "Location",    90},
        {"saya berasal dari ",   "user_city",    "Location",    90},
        {"aku tinggal di ",      "user_city",    "Location",    90},
        {"saya tinggal di ",     "user_city",    "Location",    90},
        {"rumahku di ",          "user_city",    "Location",    90},
        {"i live in ",           "user_city",    "Location",    90},
        {"asalku ",              "user_city",    "Location",    90},

        // =============================================================
        // Education
        // =============================================================
        {"aku kuliah di ",       "university",   "Education",   85},
        {"saya kuliah di ",      "university",   "Education",   85},
        {"aku mahasiswa ",       "university",   "Education",   85},
        {"i study at ",          "university",   "Education",   85},

        // =============================================================
        // Preferences
        // =============================================================
        {"bahasa favoritku ",   "favorite",      "Preferences", 75},
        {"makanan favoritku ",  "favorite",      "Preferences", 75},
        {"aku suka ",            "favorite",     "Preferences", 70},
        {"saya suka ",           "favorite",     "Preferences", 70},
        {"hobiku ",              "favorite",     "Preferences", 70},
        {"i like ",              "favorite",     "Preferences", 70},
    };

    return rules;
}

} // namespace Yuki::Memory
