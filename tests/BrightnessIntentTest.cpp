#include "../src/system/IntentDetector.h"
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
using Yuki::System::CommandType;

static void TestBrightnessUp()
{
    std::cout << "\n=== Brightness UP intents ===\n\n";

    IntentDetector detector;

    {
        auto cmd = detector.Detect("naikkan brightness");
        Test("'naikkan brightness' -> Brightness",
             cmd.type == CommandType::Brightness && cmd.parameter == -2);
    }

    {
        auto cmd = detector.Detect("naik brightness");
        Test("'naik brightness' -> Brightness",
             cmd.type == CommandType::Brightness && cmd.parameter == -2);
    }

    {
        auto cmd = detector.Detect("naikin brightness");
        Test("'naikin brightness' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("layar terang");
        Test("'layar terang' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("terangkan");
        Test("'terangkan' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("lebih terang");
        Test("'lebih terang' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("brightness naik");
        Test("'brightness naik' -> Brightness",
             cmd.type == CommandType::Brightness);
    }
}

static void TestBrightnessDown()
{
    std::cout << "\n=== Brightness DOWN intents ===\n\n";

    IntentDetector detector;

    {
        auto cmd = detector.Detect("turunkan brightness");
        Test("'turunkan brightness' -> Brightness",
             cmd.type == CommandType::Brightness && cmd.parameter == -3);
    }

    {
        auto cmd = detector.Detect("turun brightness");
        Test("'turun brightness' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("turunin brightness");
        Test("'turunin brightness' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("redupkan");
        Test("'redupkan' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("redup");
        Test("'redup' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("gelapkan");
        Test("'gelapkan' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("gelap");
        Test("'gelap' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("brightness turun");
        Test("'brightness turun' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("layar redup");
        Test("'layar redup' -> Brightness",
             cmd.type == CommandType::Brightness);
    }

    {
        auto cmd = detector.Detect("brightness kecil");
        Test("'brightness kecil' -> Brightness",
             cmd.type == CommandType::Brightness);
    }
}

static void TestBrightnessAbsolute()
{
    std::cout << "\n=== Brightness absolute intents ===\n\n";

    IntentDetector detector;

    {
        auto cmd = detector.Detect("brightness 50");
        Test("'brightness 50' -> Brightness(50)",
             cmd.type == CommandType::Brightness && cmd.parameter == 50);
    }

    {
        auto cmd = detector.Detect("brightness 20 persen");
        Test("'brightness 20 persen' -> Brightness",
             cmd.type == CommandType::Brightness && cmd.parameter == 20);
    }

    {
        auto cmd = detector.Detect("brightness penuh");
        Test("'brightness penuh' -> Brightness(100)",
             cmd.type == CommandType::Brightness && cmd.parameter == 100);
    }

    {
        auto cmd = detector.Detect("brightness maksimum");
        Test("'brightness maksimum' -> Brightness(100)",
             cmd.type == CommandType::Brightness && cmd.parameter == 100);
    }
}

static void TestNonBrightness()
{
    std::cout << "\n=== Non-brightness intents ===\n\n";

    IntentDetector detector;

    auto cmd = detector.Detect("siapa nama saya");
    Test("'siapa nama saya' -> Unknown",
         cmd.type == CommandType::Unknown);
}

int main()
{
    std::cout << "Brightness Intent Tests\n";

    TestBrightnessUp();
    TestBrightnessDown();
    TestBrightnessAbsolute();
    TestNonBrightness();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
