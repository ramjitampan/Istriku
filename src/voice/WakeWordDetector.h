#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Yuki::Voice
{

enum class WakeWordResult
{
    None,
    WakeWordOnly,
    Command
};

class WakeWordDetector
{
public:
    WakeWordDetector() = default;

    WakeWordDetector(std::initializer_list<std::string_view> words);

    void AddWakeWord(std::string_view word);
    void RemoveWakeWord(std::string_view word);
    void ClearWakeWords() noexcept;
    const std::vector<std::string>& GetWakeWords() const noexcept;

    WakeWordResult Detect(
        const std::string& text,
        std::string& commandOut) const;

    WakeWordResult Detect(const std::string& text) const;

    static std::string Normalize(std::string_view text);

private:
    static std::string ToLower(std::string_view text);
    static std::string StripPunctuation(std::string_view text);
    static std::vector<std::string> Tokenize(std::string_view text);

    std::vector<std::string> m_wakeWords{
        "yuki", "you key", "you kay", "u key", "yukki",
        "you're key", "your key", "youre key", "yuuki"
    };
};

} // namespace Yuki::Voice
