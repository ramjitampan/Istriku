#include "../src/voice/VoiceSession.h"
#include "../src/voice/WhisperRecognizer.h"
#include "../src/voice/WakeWordDetector.h"
#include "../src/voice/ISpeechRecognizer.h"

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition)
    {
        std::cout << "[PASS] " << name << "\n";
        ++testsPassed;
    }
    else
    {
        std::cout << "[FAIL] " << name << "\n";
        ++testsFailed;
    }
}

static std::string GetModelPath()
{
    const char* env = std::getenv("WHISPER_MODEL_PATH");
    if (env && *env) return env;

    const char* candidates[] = {
        "../../third_party/whisper.cpp/models/ggml-tiny.bin",
        "../third_party/whisper.cpp/models/ggml-tiny.bin",
        "third_party/whisper.cpp/models/ggml-tiny.bin",
    };

    for (auto c : candidates)
    {
        std::ifstream f(c);
        if (f.good()) return c;
    }

    return {};
}

static bool HasModel()
{
    return !GetModelPath().empty();
}

// ---------------------------------------------------------------------------
// Fake recognizer for testing state transitions without real microphone
// ---------------------------------------------------------------------------
class FakeRecognizer : public Yuki::Voice::ISpeechRecognizer
{
public:
    bool Start() override
    {
        m_running = true;
        return true;
    }
    void Stop() override
    {
        m_running = false;
    }
    bool IsRunning() const noexcept override
    {
        return m_running;
    }
    void Pause() override
    {
        m_paused = true;
        m_pauseCount++;
    }
    void Resume() override
    {
        m_paused = false;
        m_resumeCount++;
    }
    void SetRecognitionHandler(RecognitionHandler handler) override
    {
        m_handler = std::move(handler);
    }
    void SetErrorHandler(ErrorHandler handler) override
    {
        m_errorHandler = std::move(handler);
    }
    void SetMode(Yuki::Voice::RecognizerMode mode) override
    {
        m_mode = mode;
    }
    Yuki::Voice::RecognizerMode GetMode() const override
    {
        return m_mode;
    }

    void FireRecognition(const std::string& text)
    {
        if (m_handler) m_handler(text);
    }

    bool m_running = false;
    bool m_paused = false;
    int m_pauseCount = 0;
    int m_resumeCount = 0;
    Yuki::Voice::RecognizerMode m_mode = Yuki::Voice::RecognizerMode::WakeWord;
    RecognitionHandler m_handler;
    ErrorHandler m_errorHandler;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestDefaultInitialState()
{
    std::cout << "\n=== Default initial state ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [](const std::string&) -> std::string
        {
            return "";
        });

    Test("Initial state is Idle",
         session.GetState() == Yuki::Voice::VoiceSessionState::Idle);
    Test("Not running initially", !session.IsRunning());
}

static void TestStartTransitionsToListeningWakeWord()
{
    std::cout << "\n=== Start transitions to ListeningWakeWord ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();
    auto* raw = recognizer.get();

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [](const std::string&) -> std::string
        {
            return "";
        });

    bool started = session.Start();
    Test("Start returns true", started);
    Test("State is ListeningWakeWord",
         session.GetState() == Yuki::Voice::VoiceSessionState::ListeningWakeWord);
    Test("Recognizer mode is WakeWord",
         raw->GetMode() == Yuki::Voice::RecognizerMode::WakeWord);
    Test("IsRunning is true", session.IsRunning());

    session.Stop();
    Test("State is Idle after Stop",
         session.GetState() == Yuki::Voice::VoiceSessionState::Idle);
}

static void TestWakeWordOnlyTransition()
{
    std::cout << "\n=== Wake word only → Speaking (feedback) → RecordingCommand ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();
    auto* raw = recognizer.get();

    std::vector<Yuki::Voice::VoiceSessionState> states;
    std::mutex statesMutex;

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [](const std::string&) -> std::string
        {
            return "";
        });

    session.OnStateChanged(
        [&](Yuki::Voice::VoiceSessionState s)
        {
            std::lock_guard<std::mutex> lock(statesMutex);
            states.push_back(s);
        });

    session.Start();

    // Fire wake word detection (wake word only — "Yuki")
    raw->FireRecognition("Yuki");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        std::lock_guard<std::mutex> lock(statesMutex);
        // Should have transitioned through ListeningWakeWord -> Speaking
        bool foundSpeaking = false;
        for (auto s : states)
        {
            if (s == Yuki::Voice::VoiceSessionState::Speaking)
            {
                foundSpeaking = true;
                break;
            }
        }
        Test("State transitions include Speaking after wake word only",
             foundSpeaking);
    }

    // State may have moved to RecordingCommand if TTS "Ya?" completed quickly
    auto finalState = session.GetState();
    Test("State is either Speaking or RecordingCommand after wake word",
         finalState == Yuki::Voice::VoiceSessionState::Speaking ||
         finalState == Yuki::Voice::VoiceSessionState::RecordingCommand);

    Test("Pause was called for wake word detection",
         raw->m_pauseCount >= 1);
    Test("Recognizer mode is CommandRecording",
         raw->GetMode() == Yuki::Voice::RecognizerMode::CommandRecording);

    session.Stop();
}

static void TestWakeWordWithCommandTransition()
{
    std::cout << "\n=== Wake word + command → Thinking (handler) ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();
    auto* raw = recognizer.get();

    std::atomic<bool> handlerCalled{false};

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [&](const std::string& text) -> std::string
        {
            handlerCalled.store(true);
            Test("Handler receives command text", text == "buka vscode");
            return ""; // empty = no TTS reply → resumes immediately
        });

    session.Start();

    raw->FireRecognition("Yuki buka VS Code");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Test("Handler was called", handlerCalled.load());
    // Handler returned empty, so ProcessCommand resumes the recognizer
    // and goes back to ListeningWakeWord
    Test("Recognizer was paused then resumed (pauseCount=1, resumeCount=1)",
         raw->m_pauseCount == 1 && raw->m_resumeCount == 1);
    Test("State is ListeningWakeWord after empty reply",
         session.GetState() == Yuki::Voice::VoiceSessionState::ListeningWakeWord);
    Test("Recognizer mode is WakeWord (after ProcessCommand set it)",
         raw->GetMode() == Yuki::Voice::RecognizerMode::WakeWord);

    session.Stop();
}

static void TestRecordingCommandToThinking()
{
    std::cout << "\n=== RecordingCommand → Thinking (command handler) ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();
    auto* raw = recognizer.get();

    std::atomic<bool> handlerCalled{false};
    std::string receivedCommand;

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [&](const std::string& text) -> std::string
        {
            handlerCalled.store(true);
            receivedCommand = text;
            return "Membuka VS Code";
        });

    session.Start();

    // Simulate wake word only -> triggers Pause + TTS "Ya?" + CommandRecording mode
    raw->FireRecognition("Yuki");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check the synchronous state changes made during FireRecognition:
    Test("Pause was called during wake word detection",
         raw->m_pauseCount >= 1);
    Test("Mode is CommandRecording after wake word",
         raw->GetMode() == Yuki::Voice::RecognizerMode::CommandRecording);

    // State is either Speaking (before TTS completes) or RecordingCommand (after)
    // Both are valid — depends on TTS thread timing
    auto state = session.GetState();
    Test("State is either Speaking or RecordingCommand after wake word",
         state == Yuki::Voice::VoiceSessionState::Speaking ||
         state == Yuki::Voice::VoiceSessionState::RecordingCommand);

    // Wait for TTS to complete and RecordingCommand to begin
    // Then simulate command text arriving via CommandRecording decode
    auto start = std::chrono::steady_clock::now();
    while (session.GetState() != Yuki::Voice::VoiceSessionState::RecordingCommand &&
           std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start).count() < 3)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (session.GetState() == Yuki::Voice::VoiceSessionState::RecordingCommand)
    {
        // Now fire command text as if Whisper decoded it in CommandRecording mode
        raw->FireRecognition("buka VS Code");

        // FireRecognition calls OnSpeechRecognized synchronously.
        // In RecordingCommand state, it m_recognizer->Pause() then ProcessCommand.
        // ProcessCommand calls handler, gets "Membuka VS Code", sets Speaking,
        // sets mode to WakeWord, and calls TTS.Speak("Membuka VS Code").

        Test("Handler was called with command text",
             handlerCalled.load());
        Test("Received command is 'buka vscode'",
             receivedCommand == "buka vscode");
    }
    else
    {
        Test("TTS completed and RecordingCommand state was reached",
             false); // timeout
    }

    session.Stop();
}

static void TestSpeakingToListeningWakeWord()
{
    std::cout << "\n=== Speaking → ListeningWakeWord (after TTS reply done) ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();
    auto* raw = recognizer.get();

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [](const std::string&) -> std::string
        {
            return "Reply from Yuki";
        });

    session.Start();

    // Wake word + command: OnSpeechRecognized -> Pause -> ProcessCommand
    // ProcessCommand -> handler returns "Reply from Yuki" -> Speaking + TTS
    raw->FireRecognition("Yuki apa kabar");

    // After synchronous FireRecognition, state should be Speaking (TTS queued)
    // and recognizer should be paused with mode WakeWord
    Test("Pause was called for command processing",
         raw->m_pauseCount >= 1);
    Test("Recognizer mode is WakeWord (set before TTS play)",
         raw->GetMode() == Yuki::Voice::RecognizerMode::WakeWord);

    auto stateAfterCommand = session.GetState();
    Test("State is either Thinking or Speaking after command with reply",
         stateAfterCommand == Yuki::Voice::VoiceSessionState::Thinking ||
         stateAfterCommand == Yuki::Voice::VoiceSessionState::Speaking);

    // Wait for TTS to complete -> OnTtsCompleted -> Resume + ListeningWakeWord
    auto start = std::chrono::steady_clock::now();
    while (session.GetState() != Yuki::Voice::VoiceSessionState::ListeningWakeWord &&
           std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start).count() < 5)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    Test("Final state is ListeningWakeWord after TTS completes",
         session.GetState() == Yuki::Voice::VoiceSessionState::ListeningWakeWord);

    // At this point recognizer should be running (resumed) and mode = WakeWord
    Test("Recognizer is resumed after TTS completion",
         !raw->m_paused && raw->m_resumeCount >= 1);
    Test("Recognizer mode remains WakeWord",
         raw->GetMode() == Yuki::Voice::RecognizerMode::WakeWord);

    session.Stop();
}

static void TestEmptyHandlerNoReply()
{
    std::cout << "\n=== Empty handler reply → back to ListeningWakeWord ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();
    auto* raw = recognizer.get();

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [](const std::string&) -> std::string
        {
            return ""; // empty = no TTS
        });

    session.Start();

    // Wake word + command, handler returns empty
    raw->FireRecognition("Yuki matikan lampu");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // When handler returns empty, the session resumes the recognizer
    // and goes back to ListeningWakeWord
    Test("Recognizer is resumed",
         !raw->m_paused);
    Test("State is ListeningWakeWord",
         session.GetState() == Yuki::Voice::VoiceSessionState::ListeningWakeWord);
    Test("Recognizer mode is WakeWord",
         raw->GetMode() == Yuki::Voice::RecognizerMode::WakeWord);

    session.Stop();
}

static void TestNonWakeWordIgnored()
{
    std::cout << "\n=== Non-wake word text is ignored ===\n\n";

    auto recognizer = std::make_unique<FakeRecognizer>();
    auto* raw = recognizer.get();

    std::atomic<int> handlerCallCount{0};

    Yuki::Voice::VoiceSession session(
        std::move(recognizer),
        [&](const std::string&) -> std::string
        {
            handlerCallCount.fetch_add(1);
            return "";
        });

    session.Start();

    // Fire non-wake text
    raw->FireRecognition("halo apa kabar");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Test("Handler not called for non-wake text",
         handlerCallCount.load() == 0);
    Test("State remains ListeningWakeWord",
         session.GetState() == Yuki::Voice::VoiceSessionState::ListeningWakeWord);

    session.Stop();
}

static void TestWhisperRecognizerDefaultMode()
{
    std::cout << "\n=== WhisperRecognizer default mode ===\n\n";

    if (!HasModel())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    Yuki::Voice::WhisperRecognizer recognizer(GetModelPath());
    Test("Default mode is WakeWord",
         recognizer.GetMode() == Yuki::Voice::RecognizerMode::WakeWord);

    recognizer.SetMode(Yuki::Voice::RecognizerMode::CommandRecording);
    Test("SetMode to CommandRecording",
         recognizer.GetMode() == Yuki::Voice::RecognizerMode::CommandRecording);

    recognizer.SetMode(Yuki::Voice::RecognizerMode::WakeWord);
    Test("SetMode back to WakeWord",
         recognizer.GetMode() == Yuki::Voice::RecognizerMode::WakeWord);
}

int main()
{
    std::cout << "Yuki Voice Session Flow Tests\n";

    auto modelPath = GetModelPath();
    if (!modelPath.empty())
        std::cout << "WHISPER_MODEL_PATH = " << modelPath << "\n";
    else
        std::cout << "WHISPER_MODEL_PATH not found (model tests will skip)\n";

    TestDefaultInitialState();
    TestStartTransitionsToListeningWakeWord();
    TestWakeWordOnlyTransition();
    TestWakeWordWithCommandTransition();
    TestRecordingCommandToThinking();
    TestSpeakingToListeningWakeWord();
    TestEmptyHandlerNoReply();
    TestNonWakeWordIgnored();
    TestWhisperRecognizerDefaultMode();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
