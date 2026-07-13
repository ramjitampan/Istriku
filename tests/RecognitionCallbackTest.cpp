#include "../src/voice/VoiceSession.h"
#include "../src/voice/ISpeechRecognizer.h"

#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Voice::ISpeechRecognizer;
using Yuki::Voice::VoiceSession;
using Yuki::Voice::VoiceSessionState;
using Yuki::Voice::RecognizerMode;
using Yuki::Voice::WakeWordResult;

// ---------------------------------------------------------------------------
// Fake recognizer — counts how many times recognition handler fires
// ---------------------------------------------------------------------------
class CallbackCounter : public ISpeechRecognizer
{
public:
    bool Start() override { m_running = true; return true; }
    void Stop() override { m_running = false; }
    bool IsRunning() const noexcept override { return m_running; }
    void Pause() override { m_paused = true; }
    void Resume() override { m_paused = false; }
    void SetRecognitionHandler(RecognitionHandler handler) override
    {
        m_handler = std::move(handler);
    }
    void SetErrorHandler(ErrorHandler handler) override
    {
        m_errorHandler = std::move(handler);
    }
    void SetMode(RecognizerMode mode) override
    {
        m_mode = mode;
    }
    RecognizerMode GetMode() const override
    {
        return m_mode;
    }

    void FireRecognition(const std::string& text)
    {
        m_callbackCount.fetch_add(1);
        if (m_handler) m_handler(text);
    }

    bool m_running = false;
    bool m_paused = false;
    RecognizerMode m_mode = RecognizerMode::WakeWord;
    RecognitionHandler m_handler;
    ErrorHandler m_errorHandler;
    std::atomic<int> m_callbackCount{0};
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestCallbackFiresOnceForWakeWordCommand()
{
    std::cout << "\n=== Callback fires once for wake word + command ===\n\n";

    auto recognizer = std::make_unique<CallbackCounter>();
    auto* raw = recognizer.get();

    std::atomic<int> handlerCallCount{0};

    VoiceSession session(
        std::move(recognizer),
        [&](const std::string&) -> std::string
        {
            handlerCallCount.fetch_add(1);
            return ""; // empty reply
        });

    session.Start();

    // Fire wake word + command in one shot
    raw->FireRecognition("Yuki buka vscode");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Test("Handler called exactly once",
         handlerCallCount.load() == 1);
    Test("Recognizer callback fired exactly once",
         raw->m_callbackCount.load() == 1);

    session.Stop();
}

static void TestCallbackFiresOnceForWakeWordOnly()
{
    std::cout << "\n=== Callback fires once for wake word only ===\n\n";

    auto recognizer = std::make_unique<CallbackCounter>();
    auto* raw = recognizer.get();

    std::atomic<int> handlerCallCount{0};

    VoiceSession session(
        std::move(recognizer),
        [&](const std::string&) -> std::string
        {
            handlerCallCount.fetch_add(1);
            return "";
        });

    session.Start();

    // Fire wake word only — triggers TTS "Ya?"
    raw->FireRecognition("Yuki");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Handler should NOT be called for wake-word-only (not a command)
    Test("Handler NOT called for wake word only",
         handlerCallCount.load() == 0);
    Test("Recognizer callback fired exactly once",
         raw->m_callbackCount.load() == 1);

    session.Stop();
}

static void TestNoCallbackForNonWakeWord()
{
    std::cout << "\n=== No callback for non-wake word ===\n\n";

    auto recognizer = std::make_unique<CallbackCounter>();
    auto* raw = recognizer.get();

    std::atomic<int> handlerCallCount{0};

    VoiceSession session(
        std::move(recognizer),
        [&](const std::string&) -> std::string
        {
            handlerCallCount.fetch_add(1);
            return "";
        });

    session.Start();

    raw->FireRecognition("halo apa kabar");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Test("Handler NOT called for non-wake text",
         handlerCallCount.load() == 0);
    Test("Recognizer callback fired exactly once",
         raw->m_callbackCount.load() == 1);

    session.Stop();
}

static void TestCallbackFiresOnceForEmptyText()
{
    std::cout << "\n=== Empty text does not fire handler ===\n\n";

    auto recognizer = std::make_unique<CallbackCounter>();
    auto* raw = recognizer.get();

    std::atomic<int> handlerCallCount{0};

    VoiceSession session(
        std::move(recognizer),
        [&](const std::string&) -> std::string
        {
            handlerCallCount.fetch_add(1);
            return "";
        });

    session.Start();

    // Empty text should be ignored by OnSpeechRecognized
    raw->FireRecognition("");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Test("Handler NOT called for empty text",
         handlerCallCount.load() == 0);
    Test("Recognizer callback still fires for empty (filtered by VoiceSession)",
         raw->m_callbackCount.load() == 1);

    session.Stop();
}

int main()
{
    std::cout << "Recognition Callback Tests\n";

    TestCallbackFiresOnceForWakeWordCommand();
    TestCallbackFiresOnceForWakeWordOnly();
    TestNoCallbackForNonWakeWord();
    TestCallbackFiresOnceForEmptyText();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
