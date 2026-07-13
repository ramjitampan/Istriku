#pragma once

#include "ISpeechRecognizer.h"

#include "../config/Config.h"

#include <memory>
#include <string_view>

namespace Yuki::Voice
{

class WhisperRecognizer final : public ISpeechRecognizer
{
public:
    explicit WhisperRecognizer(std::string_view modelPath);
    explicit WhisperRecognizer(const Yuki::Config::VoiceConfig& config);
    ~WhisperRecognizer() override;

    WhisperRecognizer(const WhisperRecognizer&) = delete;
    WhisperRecognizer& operator=(const WhisperRecognizer&) = delete;

    WhisperRecognizer(WhisperRecognizer&&) noexcept;
    WhisperRecognizer& operator=(WhisperRecognizer&&) noexcept;

    bool Start() override;
    void Stop() override;
    bool IsRunning() const noexcept override;

    void Pause() override;
    void Resume() override;

    void SetRecognitionHandler(RecognitionHandler handler) override;
    void SetErrorHandler(ErrorHandler handler) override;

    void SetMode(RecognizerMode mode) override;
    RecognizerMode GetMode() const override;

    void EnrollSpeaker(const std::vector<float>& audioSamples);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Yuki::Voice
