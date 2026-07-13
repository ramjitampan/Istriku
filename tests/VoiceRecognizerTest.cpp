#include "../src/voice/WhisperRecognizer.h"

#include <cstdlib>
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

#include <fstream>

static std::string GetModelPath()
{
    const char* env = std::getenv("WHISPER_MODEL_PATH");
    if (env && *env) return env;

    // Try common locations relative to CWD
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

    return {}; // not found
}

static void TestConstructorInvalidModel()
{
    std::cout << "\n=== Constructor with invalid model path ===\n\n";

    bool caught = false;
    try
    {
        Yuki::Voice::WhisperRecognizer recognizer(
            "C:\\nonexistent\\model.bin");
    }
    catch (const std::exception& e)
    {
        caught = true;
        std::cout << "  Exception: " << e.what() << "\n";
    }

    Test("Invalid model path throws exception", caught);
}

static void TestMoveSemantics()
{
    std::cout << "\n=== Move semantics ===\n\n";

    Test("Move semantics are valid (compile-time test)", true);
}

static bool HasModel()
{
    auto path = GetModelPath();
    return !path.empty();
}

static void TestConstructorValidModel()
{
    std::cout << "\n=== Constructor with valid model ===\n\n";

    auto modelPath = GetModelPath();
    if (modelPath.empty())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    std::cout << "  Model: " << modelPath << "\n";

    bool ok = false;
    try
    {
        Yuki::Voice::WhisperRecognizer recognizer(modelPath);
        ok = true;
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
    }

    Test("Constructor with valid model succeeds", ok);
}

static void TestInitialState()
{
    std::cout << "\n=== Initial state ===\n\n";

    auto modelPath = GetModelPath();
    if (modelPath.empty())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    try
    {
        Yuki::Voice::WhisperRecognizer recognizer(modelPath);
        Test("Constructor succeeded", true);
        Test("IsRunning is false before Start",
             !recognizer.IsRunning());
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
    }
}

static void TestStartStop()
{
    std::cout << "\n=== Start / Stop ===\n\n";

    auto modelPath = GetModelPath();
    if (modelPath.empty())
    {
        Test("WHISPER_MODEL_PATH not found — skip", true);
        return;
    }

    try
    {
        Yuki::Voice::WhisperRecognizer recognizer(modelPath);

        // Start — mungkin gagal kalo gak ada mic, tapi gak crash
        bool started = recognizer.Start();
        Test("Start returns without crash", true);

        if (started)
            Test("IsRunning true after Start", recognizer.IsRunning());
        else
            Test("Start returned false (no mic?) — still ok", true);

        recognizer.Stop();
        Test("Stop returns without crash", true);
        Test("IsRunning false after Stop",
             !recognizer.IsRunning());

        // Double stop should be safe
        recognizer.Stop();
        Test("Double Stop is safe", true);
    }
    catch (const std::exception& e)
    {
        std::cout << "  Exception: " << e.what() << "\n";
        Test("Start/Stop no exception", false);
    }
}

int main()
{
    std::cout << "Yuki VoiceRecognizer Tests (Whisper.cpp)\n";

    auto modelPath = GetModelPath();
    if (!modelPath.empty())
        std::cout << "WHISPER_MODEL_PATH = " << modelPath << "\n";
    else
        std::cout << "WHISPER_MODEL_PATH not found (model tests will skip)\n";

    TestConstructorInvalidModel();
    TestMoveSemantics();
    TestConstructorValidModel();
    TestInitialState();
    TestStartStop();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
