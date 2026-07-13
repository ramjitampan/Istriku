#include "../src/voice/WakeWordDetector.h"

#include <iostream>
#include <string>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Voice::WakeWordDetector;
using Yuki::Voice::WakeWordResult;

static void TestWakeWordVariants()
{
    std::cout << "\n=== Wake word text variants ===\n\n";

    WakeWordDetector detector;

    // Basic matching
    {
        std::string cmd;
        auto result = detector.Detect("yuki", cmd);
        Test("'yuki' -> WakeWordOnly", result == WakeWordResult::WakeWordOnly);
    }

    {
        std::string cmd;
        auto result = detector.Detect("YUKI", cmd);
        Test("'YUKI' -> WakeWordOnly", result == WakeWordResult::WakeWordOnly);
    }

    {
        std::string cmd;
        auto result = detector.Detect("Hello Yuki", cmd);
        Test("'Hello Yuki' -> WakeWordOnly", result == WakeWordResult::WakeWordOnly);
    }

    {
        std::string cmd;
        auto result = detector.Detect("Yuki,", cmd);
        Test("'Yuki,' -> WakeWordOnly", result == WakeWordResult::WakeWordOnly);
    }

    {
        std::string cmd;
        auto result = detector.Detect("Hello, Yuki", cmd);
        Test("'Hello, Yuki' -> WakeWordOnly", result == WakeWordResult::WakeWordOnly);
    }
}

static void TestWakeWordWithPunctuation()
{
    std::cout << "\n=== Wake word with punctuation ===\n\n";

    WakeWordDetector detector;

    {
        std::string cmd;
        auto result = detector.Detect("Yuki buka vscode", cmd);
        Test("'Yuki buka vscode' -> Command", result == WakeWordResult::Command);
        Test("cmd = 'buka vscode'", cmd == "buka vscode");
    }

    {
        std::string cmd;
        auto result = detector.Detect("Yuki, buka notepad", cmd);
        Test("'Yuki, buka notepad' -> Command", result == WakeWordResult::Command);
        Test("cmd = 'buka notepad'", cmd == "buka notepad");
    }

    {
        std::string cmd;
        auto result = detector.Detect("Hello, Yuki! buka chrome", cmd);
        Test("'Hello, Yuki! buka chrome' -> Command", result == WakeWordResult::Command);
        Test("cmd = 'buka chrome'", cmd == "buka chrome");
    }

    {
        std::string cmd;
        auto result = detector.Detect("YUKI? apa kabar", cmd);
        Test("'YUKI? apa kabar' -> Command", result == WakeWordResult::Command);
        Test("cmd = 'apa kabar'", cmd == "apa kabar");
    }
}

static void TestWakeWordInSentence()
{
    std::cout << "\n=== Wake word in sentence ===\n\n";

    WakeWordDetector detector;

    {
        std::string cmd;
        auto result = detector.Detect("halo yuki apa kabar", cmd);
        Test("'halo yuki apa kabar' -> Command", result == WakeWordResult::Command);
        Test("cmd = 'apa kabar'", cmd == "apa kabar");
    }

    {
        std::string cmd;
        auto result = detector.Detect("hey you key buka notepad", cmd);
        Test("'hey you key buka notepad' -> Command", result == WakeWordResult::Command);
        Test("cmd = 'buka notepad'", cmd == "buka notepad");
    }
}

static void TestNonWakeWords()
{
    std::cout << "\n=== Non-wake words ===\n\n";

    WakeWordDetector detector;

    Test("'halo' -> None",
         detector.Detect("halo") == WakeWordResult::None);
    Test("'yuk' -> None",
         detector.Detect("yuk") == WakeWordResult::None);
    Test("'yukii' -> None",
         detector.Detect("yukii") == WakeWordResult::None);
    Test("'keyboard' -> None",
         detector.Detect("keyboard") == WakeWordResult::None);
    Test("empty string -> None",
         detector.Detect("") == WakeWordResult::None);
    Test("'you okay' -> None",
         detector.Detect("you okay") == WakeWordResult::None);
}

static void TestNormalize()
{
    std::cout << "\n=== Static Normalize ===\n\n";

    Test("Normalize lowercase",
         WakeWordDetector::Normalize("Yuki") == "yuki");
    Test("Normalize mixed case",
         WakeWordDetector::Normalize("You're Key") == "you're key");
    Test("Normalize empty",
         WakeWordDetector::Normalize("").empty());
}

int main()
{
    std::cout << "Wake Word Normalization Tests\n";

    TestWakeWordVariants();
    TestWakeWordWithPunctuation();
    TestWakeWordInSentence();
    TestNonWakeWords();
    TestNormalize();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
