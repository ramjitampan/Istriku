#pragma once

#include <functional>
#include <string>

namespace Yuki::Voice
{

enum class RecognizerMode
{
    WakeWord = 0,
    CommandRecording = 1
};

class ISpeechRecognizer
{
public:
    using RecognitionHandler = std::function<void(std::string)>;
    using ErrorHandler = std::function<void(std::string)>;

    virtual ~ISpeechRecognizer() = default;

    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const noexcept = 0;

    virtual void Pause() = 0;
    virtual void Resume() = 0;

    virtual void SetRecognitionHandler(RecognitionHandler handler) = 0;
    virtual void SetErrorHandler(ErrorHandler handler) = 0;

    virtual void SetMode(RecognizerMode mode) = 0;
    virtual RecognizerMode GetMode() const = 0;
};

} // namespace Yuki::Voice
