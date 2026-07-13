#pragma once

#include "../config/Config.h"

#include <functional>
#include <memory>
#include <string>

namespace Yuki::Voice
{

class TextToSpeech
{
public:
    using CompletionHandler = std::function<void()>;

    TextToSpeech();
    explicit TextToSpeech(const Yuki::Config::TTSConfig& config);
    ~TextToSpeech();

    TextToSpeech(const TextToSpeech&) = delete;
    TextToSpeech& operator=(const TextToSpeech&) = delete;

    TextToSpeech(TextToSpeech&&) noexcept;
    TextToSpeech& operator=(TextToSpeech&&) noexcept;

    void Speak(const std::string& text);
    void Stop();

    void SetVolume(int volume); // 0-100
    int GetVolume() const noexcept;

    void SetRate(int rate); // -10 .. 10
    int GetRate() const noexcept;

    void Mute(bool mute);
    bool IsMuted() const noexcept;

    bool IsSpeaking() const noexcept;

    void SetCompletionHandler(CompletionHandler handler);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Yuki::Voice
