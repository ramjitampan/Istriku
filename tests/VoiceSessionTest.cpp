#include "../src/voice/VoiceSession.h"
#include "../src/voice/WhisperRecognizer.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

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

static void TestCreateSession()
{
    std::cout << "\n=== Create VoiceSession ===\n\n";

    if (!HasModel())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    auto modelPath = GetModelPath();

    bool ok = false;
    try
    {
        auto recognizer =
            std::make_unique<Yuki::Voice::WhisperRecognizer>(modelPath);

        Yuki::Voice::VoiceSession session(
            std::move(recognizer),
            [](const std::string& text) -> std::string
            {
                return "Reply: " + text;
            });

        ok = true;
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
    }

    Test("VoiceSession created successfully", ok);
}

static void TestInitialState()
{
    std::cout << "\n=== Initial state ===\n\n";

    if (!HasModel())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    auto modelPath = GetModelPath();

    try
    {
        auto recognizer =
            std::make_unique<Yuki::Voice::WhisperRecognizer>(modelPath);

        Yuki::Voice::VoiceSession session(
            std::move(recognizer),
            [](const std::string& text) -> std::string
            {
                return "Reply: " + text;
            });

        Test("Default listening mode is AlwaysOn",
             session.GetListeningMode() ==
             Yuki::Voice::ListeningMode::AlwaysOn);

        Test("Default state is Idle",
             session.GetState() ==
             Yuki::Voice::VoiceSessionState::Idle);

        Test("Not running initially",
             !session.IsRunning());
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
    }
}

static void TestStartStop()
{
    std::cout << "\n=== Start / Stop ===\n\n";

    if (!HasModel())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    auto modelPath = GetModelPath();

    try
    {
        auto recognizer =
            std::make_unique<Yuki::Voice::WhisperRecognizer>(modelPath);

        Yuki::Voice::VoiceSession session(
            std::move(recognizer),
            [](const std::string& text) -> std::string
            {
                return "Reply: " + text;
            });

        bool started = session.Start();
        Test("Start returns without crash", true);

        if (started)
            Test("IsRunning after Start", session.IsRunning());

        session.Stop();
        Test("Stop without crash", true);
        Test("IsRunning false after Stop",
             !session.IsRunning());
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
        Test("Start/Stop no exception", false);
    }
}

static void TestVolumeAndRatePassthrough()
{
    std::cout << "\n=== Volume and Rate passthrough ===\n\n";

    if (!HasModel())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    auto modelPath = GetModelPath();

    try
    {
        auto recognizer =
            std::make_unique<Yuki::Voice::WhisperRecognizer>(modelPath);

        Yuki::Voice::VoiceSession session(
            std::move(recognizer),
            [](const std::string& text) -> std::string
            {
                return "Reply: " + text;
            });

        session.SetVolume(50);
        Test("Volume = 50", session.GetVolume() == 50);

        session.SetRate(-3);
        Test("Rate = -3", session.GetRate() == -3);

        session.Mute(true);
        Test("Muted", session.IsMuted());

        session.Mute(false);
        Test("Not muted", !session.IsMuted());
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
    }
}

static void TestStateTransitions()
{
    std::cout << "\n=== State transitions ===\n\n";

    if (!HasModel())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    auto modelPath = GetModelPath();

    try
    {
        auto recognizer =
            std::make_unique<Yuki::Voice::WhisperRecognizer>(modelPath);

        Yuki::Voice::VoiceSession session(
            std::move(recognizer),
            [](const std::string& text) -> std::string
            {
                return "Reply: " + text;
            });

        // Idle before start
        Test("State is Idle before Start",
             session.GetState() ==
             Yuki::Voice::VoiceSessionState::Idle);

        bool started = session.Start();
        if (started)
        {
            // Should be ListeningWakeWord after Start
            Test("State is ListeningWakeWord after Start",
                 session.GetState() ==
                 Yuki::Voice::VoiceSessionState::ListeningWakeWord);
        }

        session.Stop();
        Test("State is Idle after Stop",
             session.GetState() ==
             Yuki::Voice::VoiceSessionState::Idle);
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
    }
}

static void TestStateChangedCallback()
{
    std::cout << "\n=== State changed callback ===\n\n";

    if (!HasModel())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    auto modelPath = GetModelPath();

    try
    {
        auto recognizer =
            std::make_unique<Yuki::Voice::WhisperRecognizer>(modelPath);

        Yuki::Voice::VoiceSession session(
            std::move(recognizer),
            [](const std::string& text) -> std::string
            {
                return "Reply: " + text;
            });

        bool callbackFired = false;

        session.OnStateChanged(
            [&](Yuki::Voice::VoiceSessionState)
            {
                callbackFired = true;
            });

        bool started = session.Start();
        if (started)
        {
            Test("State callback fires on Start",
                 callbackFired);
        }

        session.Stop();
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
    }
}

int main()
{
    std::cout << "Yuki VoiceSession Tests\n";

    auto modelPath = GetModelPath();
    if (!modelPath.empty())
        std::cout << "WHISPER_MODEL_PATH = " << modelPath << "\n";
    else
        std::cout << "WHISPER_MODEL_PATH not found (model tests will skip)\n";

    TestCreateSession();
    TestInitialState();
    TestStartStop();
    TestVolumeAndRatePassthrough();
    TestStateTransitions();
    TestStateChangedCallback();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
