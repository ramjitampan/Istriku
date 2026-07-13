#pragma once

#include "ISpeechRecognizer.h"
#include "TextToSpeech.h"
#include "WakeWordDetector.h"

#include <functional>
#include <memory>
#include <string>

namespace Yuki::Voice
{

enum class ListeningMode
{
    PushToTalk,
    AlwaysOn
};

enum class VoiceSessionState
{
    Idle,
    ListeningWakeWord,
    RecordingCommand,
    Thinking,
    Speaking
};

class VoiceSession
{
public:
    using CommandHandler =
        std::function<std::string(const std::string&)>;
    using StateHandler =
        std::function<void(VoiceSessionState)>;

    VoiceSession(
        std::unique_ptr<ISpeechRecognizer> recognizer,
        CommandHandler handler);

    ~VoiceSession();

    VoiceSession(const VoiceSession&) = delete;
    VoiceSession& operator=(const VoiceSession&) = delete;

    VoiceSession(VoiceSession&&) noexcept = default;
    VoiceSession& operator=(VoiceSession&&) noexcept = default;

    bool Start();
    void Stop();
    bool IsRunning() const noexcept;

    void PushToTalk();
    void SetListeningMode(ListeningMode mode);
    ListeningMode GetListeningMode() const noexcept;

    VoiceSessionState GetState() const noexcept;
    void OnStateChanged(StateHandler handler);

    void SetVolume(int volume);
    int GetVolume() const noexcept;
    void SetRate(int rate);
    int GetRate() const noexcept;
    void Mute(bool mute);
    bool IsMuted() const noexcept;
    bool IsSpeaking() const noexcept;

private:
    void OnSpeechRecognized(const std::string& text);
    void OnTtsCompleted();
    void SetState(VoiceSessionState state);

    void SetRecognizerMode(RecognizerMode mode);
    void ProcessCommand(const std::string& command);

    std::unique_ptr<ISpeechRecognizer> m_recognizer;
    WakeWordDetector m_wakeWord;
    TextToSpeech m_tts;

    CommandHandler m_handler;
    StateHandler m_stateHandler;

    ListeningMode m_mode = ListeningMode::AlwaysOn;
    VoiceSessionState m_state = VoiceSessionState::Idle;
};

} // namespace Yuki::Voice
