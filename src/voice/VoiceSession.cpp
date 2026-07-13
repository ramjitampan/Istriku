#include "VoiceSession.h"
#include "utils/Logger.h"
#include "utils/Utf8Sanitizer.h"
#include "utils/TextNormalizer.h"

#include <iostream>
#include <utility>

namespace Yuki::Voice
{

VoiceSession::VoiceSession(
    std::unique_ptr<ISpeechRecognizer> recognizer,
    CommandHandler handler)
    : m_recognizer(std::move(recognizer))
    , m_handler(std::move(handler))
{
    m_recognizer->SetRecognitionHandler(
        [this](const std::string& text)
        {
            OnSpeechRecognized(text);
        });

    m_recognizer->SetErrorHandler(
        [this](const std::string& error)
        {
            Logger::Error("[Voice] " + error);
            if (error.find("Maaf") != std::string::npos)
            {
                SetState(VoiceSessionState::Speaking);
                m_tts.Speak(error);
            }
        });

    m_tts.SetCompletionHandler(
        [this]
        {
            OnTtsCompleted();
        });
}

VoiceSession::~VoiceSession()
{
    Stop();
    m_tts.SetCompletionHandler(nullptr);
}

bool VoiceSession::Start()
{
    SetState(VoiceSessionState::ListeningWakeWord);
    SetRecognizerMode(RecognizerMode::WakeWord);

    try
    {
        if (!m_recognizer->Start())
        {
            Logger::Error("[Voice] Gagal memulai recognizer");
            SetState(VoiceSessionState::Idle);
            return false;
        }
    }
    catch (const std::exception& e)
    {
        Logger::Error("[Voice] Start exception: " + std::string(e.what()));
        SetState(VoiceSessionState::Idle);
        return false;
    }

    Logger::Info("[Voice] Listening Wake Word...");
    return true;
}

void VoiceSession::Stop()
{
    m_recognizer->Stop();
    m_tts.Stop();
    SetState(VoiceSessionState::Idle);
}

bool VoiceSession::IsRunning() const noexcept
{
    return m_recognizer->IsRunning();
}

void VoiceSession::PushToTalk()
{
    if (m_mode != ListeningMode::PushToTalk) return;

    if (!m_recognizer->IsRunning())
        m_recognizer->Start();

    SetState(VoiceSessionState::ListeningWakeWord);
    SetRecognizerMode(RecognizerMode::WakeWord);
    std::cout << "[Voice] Listening Wake Word...\n";
}

void VoiceSession::SetListeningMode(ListeningMode mode)
{
    m_mode = mode;
}

ListeningMode VoiceSession::GetListeningMode() const noexcept
{
    return m_mode;
}

VoiceSessionState VoiceSession::GetState() const noexcept
{
    return m_state;
}

void VoiceSession::OnStateChanged(StateHandler handler)
{
    m_stateHandler = std::move(handler);
}

void VoiceSession::SetVolume(int volume)
{
    m_tts.SetVolume(volume);
}

int VoiceSession::GetVolume() const noexcept
{
    return m_tts.GetVolume();
}

void VoiceSession::SetRate(int rate)
{
    m_tts.SetRate(rate);
}

int VoiceSession::GetRate() const noexcept
{
    return m_tts.GetRate();
}

void VoiceSession::Mute(bool mute)
{
    m_tts.Mute(mute);
}

bool VoiceSession::IsMuted() const noexcept
{
    return m_tts.IsMuted();
}

bool VoiceSession::IsSpeaking() const noexcept
{
    return m_tts.IsSpeaking();
}

void VoiceSession::SetRecognizerMode(RecognizerMode mode)
{
    m_recognizer->SetMode(mode);
}

void VoiceSession::ProcessCommand(const std::string& command)
{
    Logger::Info("[Voice] Sending command to handler: \"" + command + "\"");
    SetState(VoiceSessionState::Thinking);

    std::string reply;
    try
    {
        if (m_handler)
        {
            reply = m_handler(command);
        }
    }
    catch (const std::exception& e)
    {
        Logger::Error("[Voice] Handler exception: " + std::string(e.what()));
        reply = "Maaf, terjadi kesalahan.";
    }

    if (!reply.empty())
    {
        Logger::Info("[Voice] Got reply: \"" + reply + "\"");
        SetState(VoiceSessionState::Speaking);
        SetRecognizerMode(RecognizerMode::WakeWord);
        m_tts.Speak(reply);
    }
    else
    {
        Logger::Info("[Voice] No reply — back to listening");
        SetRecognizerMode(RecognizerMode::WakeWord);
        m_recognizer->Resume();
        SetState(VoiceSessionState::ListeningWakeWord);
    }
}

void VoiceSession::OnSpeechRecognized(const std::string& text)
{
    if (text.empty()) return;

    // Step 1: Sanitize UTF-8
    std::string sanitized = Yuki::Utils::Utf8Sanitizer::Sanitize(text);

    // Step 2: Normalize with recognition pipeline
    std::string normalized = Yuki::Utils::TextNormalizer::NormalizeRecognition(sanitized);

    if (normalized.empty()) return;

    if (m_state == VoiceSessionState::RecordingCommand)
    {
        Logger::Info("[Voice] Recognized: \"" + normalized + "\"");
        m_recognizer->Pause();
        ProcessCommand(normalized);
        return;
    }

    // We are in ListeningWakeWord — check for wake word
    std::string command;
    auto result = m_wakeWord.Detect(normalized, command);

    if (result == WakeWordResult::None)
    {
        return;
    }

    Logger::Info("[Voice] Wake Word Detected");

    if (result == WakeWordResult::WakeWordOnly)
    {
        Logger::Info("[Voice] Wake word only — preparing to record command");
        m_recognizer->Pause();
        SetState(VoiceSessionState::Speaking);
        SetRecognizerMode(RecognizerMode::CommandRecording);
        m_tts.Speak("Ya?");
        return;
    }

    // Wake word + command in one utterance
    Logger::Info("[Voice] Recognized: \"" + command + "\"");
    m_recognizer->Pause();
    ProcessCommand(command);
}

void VoiceSession::OnTtsCompleted()
{
    switch (m_state)
    {
        case VoiceSessionState::Speaking:
        {
            if (m_recognizer->GetMode() == RecognizerMode::CommandRecording)
            {
                m_recognizer->Resume();
                Logger::Info("[Voice] Recording Command...");
                SetState(VoiceSessionState::RecordingCommand);
            }
            else
            {
                m_recognizer->Resume();
                Logger::Info("[Voice] Back to Listening");
                SetState(VoiceSessionState::ListeningWakeWord);
            }
            break;
        }
        default:
            break;
    }
}

void VoiceSession::SetState(VoiceSessionState state)
{
    if (m_state == state) return;
    m_state = state;
    if (m_stateHandler)
        m_stateHandler(state);
}

} // namespace Yuki::Voice
