#include "../src/system/IntentDetector.h"
#include "../src/system/WindowsController.h"
#include "../src/system/Command.h"

#include <iostream>
#include <string>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::System::IntentDetector;
using Yuki::System::WindowsController;
using Yuki::System::CommandType;
using Yuki::System::Command;

// This test verifies the routing logic:
//   IntentDetector -> Desktop command? -> WindowsController (NOT AI)
//   IntentDetector -> No command? -> AI

static void TestDesktopCommandsAreNotUnknown()
{
    std::cout << "\n=== Desktop commands are detected ===\n\n";

    IntentDetector detector;

    auto cmd = detector.Detect("buka vscode");
    Test("'buka vscode' is not Unknown",
         cmd.type != CommandType::Unknown);

    cmd = detector.Detect("turunkan brightness");
    Test("'turunkan brightness' is not Unknown",
         cmd.type != CommandType::Unknown);

    cmd = detector.Detect("naikkan volume");
    Test("'naikkan volume' is not Unknown",
         cmd.type != CommandType::Unknown);

    cmd = detector.Detect("screenshot");
    Test("'screenshot' is not Unknown",
         cmd.type != CommandType::Unknown);

    cmd = detector.Detect("matikan laptop");
    Test("'matikan laptop' is not Unknown",
         cmd.type != CommandType::Unknown);
}

static void TestChatCommandsAreUnknown()
{
    std::cout << "\n=== Chat commands are Unknown (routed to AI) ===\n\n";

    IntentDetector detector;

    auto cmd = detector.Detect("siapa nama saya");
    Test("'siapa nama saya' is Unknown",
         cmd.type == CommandType::Unknown);

    cmd = detector.Detect("apa kabar");
    Test("'apa kabar' is Unknown",
         cmd.type == CommandType::Unknown);

    cmd = detector.Detect("ceritakan tentang dirimu");
    Test("'ceritakan tentang dirimu' is Unknown",
         cmd.type == CommandType::Unknown);
}

static void TestVolumeBrightnessDisambiguation()
{
    std::cout << "\n=== Volume vs Brightness disambiguation ===\n\n";

    IntentDetector detector;

    // "turunkan brightness" should NOT be detected as Volume
    auto cmd = detector.Detect("turunkan brightness");
    Test("'turunkan brightness' is Brightness, not Volume",
         cmd.type == CommandType::Brightness);

    // "volume turun" should be Volume
    cmd = detector.Detect("volume turun");
    Test("'volume turun' is Volume, not Brightness",
         cmd.type == CommandType::Volume);

    // Mixed sentences
    cmd = detector.Detect("tolong turunkan brightness");
    Test("'tolong turunkan brightness' is Brightness",
         cmd.type == CommandType::Brightness);

    cmd = detector.Detect("tolong naikkan volume");
    Test("'tolong naikkan volume' is Volume",
         cmd.type == CommandType::Volume);
}

int main()
{
    std::cout << "Voice Routing Tests\n";

    TestDesktopCommandsAreNotUnknown();
    TestChatCommandsAreUnknown();
    TestVolumeBrightnessDisambiguation();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
