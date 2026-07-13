#include "../src/voice/WakeWordDetector.h"

#include <algorithm>
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

static void TestDefaultWakeWords()
{
    std::cout << "\n=== Default Wake Words ===\n\n";

    Yuki::Voice::WakeWordDetector detector;
    const auto& words = detector.GetWakeWords();

    Test("Default wake words count == 9",
         words.size() == 9);
    Test("Contains 'yuki'",
         words.size() > 0 && words[0] == "yuki");
    Test("Contains 'you're key'",
         std::find(words.begin(), words.end(), std::string("you're key")) != words.end());
}

static void TestDetectYuki()
{
    std::cout << "\n=== Detect 'Yuki' ===\n\n";

    Yuki::Voice::WakeWordDetector detector;

    // Exact match
    {
        std::string cmd;
        auto result = detector.Detect("Yuki", cmd);
        Test("'Yuki' -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
        Test("'Yuki' -> cmd kosong",
             cmd.empty());
    }

    // Lowercase
    {
        std::string cmd;
        auto result = detector.Detect("yuki", cmd);
        Test("'yuki' -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }

    // Uppercase
    {
        std::string cmd;
        auto result = detector.Detect("YUKI", cmd);
        Test("'YUKI' -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }

    // Wake word with command
    {
        std::string cmd;
        auto result = detector.Detect("Yuki buka vscode", cmd);
        Test("'Yuki buka vscode' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'buka vscode'",
             cmd == "buka vscode");
    }

    // Wake word with leading spaces
    {
        std::string cmd;
        auto result = detector.Detect("  Yuki", cmd);
        Test("'  Yuki' (leading spaces) -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }
}

static void TestPunctuation()
{
    std::cout << "\n=== Punctuation handling ===\n\n";

    Yuki::Voice::WakeWordDetector detector;

    // Koma sebelum Yuki — ini yg gagal di log sebelumnya!
    {
        std::string cmd;
        auto result = detector.Detect("Halo, Yuki buka kalkulator", cmd);
        Test("'Halo, Yuki buka kalkulator' -> Command (koma)",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'buka kalkulator'",
             cmd == "buka kalkulator");
    }

    // Koma setelah Yuki
    {
        std::string cmd;
        auto result = detector.Detect("Yuki, buka notepad", cmd);
        Test("'Yuki, buka notepad' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'buka notepad'",
             cmd == "buka notepad");
    }

    // Tanda seru
    {
        std::string cmd;
        auto result = detector.Detect("Yuki! tolong", cmd);
        Test("'Yuki! tolong' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'tolong'",
             cmd == "tolong");
    }

    // Tanda tanya
    {
        std::string cmd;
        auto result = detector.Detect("Yuki? apa kabar", cmd);
        Test("'Yuki? apa kabar' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
    }

    // Period (titik) — seperti "You're key."
    {
        std::string cmd;
        auto result = detector.Detect("You're key.", cmd);
        Test("'You're key.' -> WakeWordOnly (dengan titik)",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }

    // Titik setelah Yuki
    {
        std::string cmd;
        auto result = detector.Detect("Yuki. buka chrome", cmd);
        Test("'Yuki. buka chrome' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
    }

    // Apostrophe — "you're key"
    {
        std::string cmd;
        auto result = detector.Detect("You're key buka steam", cmd);
        Test("'You're key buka steam' (apostrophe) -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'buka steam'",
             cmd == "buka steam");
    }

    // Titik dua
    {
        std::string cmd;
        auto result = detector.Detect("Yuki: buka file", cmd);
        Test("'Yuki: buka file' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
    }
}

static void TestDetectYouKey()
{
    std::cout << "\n=== Detect 'you key' (whisper misrecognition) ===\n\n";

    Yuki::Voice::WakeWordDetector detector;

    {
        std::string cmd;
        auto result = detector.Detect("You key", cmd);
        Test("'You key' -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }

    {
        std::string cmd;
        auto result = detector.Detect("You key buka steam", cmd);
        Test("'You key buka steam' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'buka steam'",
             cmd == "buka steam");
    }
}

static void TestDetectYoureKey()
{
    std::cout << "\n=== Detect 'you're key' ===\n\n";

    Yuki::Voice::WakeWordDetector detector;

    {
        std::string cmd;
        auto result = detector.Detect("You're key", cmd);
        Test("'You're key' -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }

    {
        std::string cmd;
        auto result = detector.Detect("You're key screenshot", cmd);
        Test("'You're key screenshot' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'screenshot'",
             cmd == "screenshot");
    }

    // Dengan titik di akhir
    {
        std::string cmd;
        auto result = detector.Detect("You're key.", cmd);
        Test("'You're key.' -> WakeWordOnly (dengan titik)",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }
}

static void TestDetectYourKey()
{
    std::cout << "\n=== Detect 'your key' ===\n\n";

    Yuki::Voice::WakeWordDetector detector;

    {
        std::string cmd;
        auto result = detector.Detect("Your key", cmd);
        Test("'Your key' -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }

    {
        std::string cmd;
        auto result = detector.Detect("your key buka file", cmd);
        Test("'your key buka file' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
    }
}

static void TestDetectWakeWordInMiddle()
{
    std::cout << "\n=== Wake word di tengah kalimat ===\n\n";

    Yuki::Voice::WakeWordDetector detector;

    {
        std::string cmd;
        auto result = detector.Detect("hello Yuki apa kabar", cmd);
        Test("'hello Yuki apa kabar' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'apa kabar'",
             cmd == "apa kabar");
    }

    {
        std::string cmd;
        auto result = detector.Detect("hey you key buka notepad", cmd);
        Test("'hey you key buka notepad' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'buka notepad'",
             cmd == "buka notepad");
    }

    // Dengan koma di tengah
    {
        std::string cmd;
        auto result = detector.Detect("halo, you're key, tolong matikan laptop", cmd);
        Test("'halo, you're key, tolong matikan laptop' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'tolong matikan laptop'",
             cmd == "tolong matikan laptop");
    }
}

static void TestIgnoreNonWake()
{
    std::cout << "\n=== Non-wake words ignored ===\n\n";

    Yuki::Voice::WakeWordDetector detector;

    {
        auto result = detector.Detect("halo apa kabar");
        Test("'halo apa kabar' -> None",
             result == Yuki::Voice::WakeWordResult::None);
    }

    {
        auto result = detector.Detect("matikan laptop");
        Test("'matikan laptop' -> None",
             result == Yuki::Voice::WakeWordResult::None);
    }

    {
        auto result = detector.Detect("");
        Test("'' (empty) -> None",
             result == Yuki::Voice::WakeWordResult::None);
    }

    // "yuk" bukan "yuki"
    {
        auto result = detector.Detect("yuk jalan");
        Test("'yuk jalan' -> None (bukan wake word)",
             result == Yuki::Voice::WakeWordResult::None);
    }

    // "yukii" bukan "yuki"
    {
        auto result = detector.Detect("yukii");
        Test("'yukii' -> None (bukan 'yuki')",
             result == Yuki::Voice::WakeWordResult::None);
    }

    // "keyboard" bukan "you key"
    {
        auto result = detector.Detect("keyboard");
        Test("'keyboard' -> None (bukan 'you key')",
             result == Yuki::Voice::WakeWordResult::None);
    }

    // "you okay" bukan wake word
    {
        auto result = detector.Detect("you okay");
        Test("'you okay' -> None (bukan wake word)",
             result == Yuki::Voice::WakeWordResult::None);
    }
}

static void TestCustomWakeWords()
{
    std::cout << "\n=== Custom wake words ===\n\n";

    Yuki::Voice::WakeWordDetector detector({
        "komputer",
        "assistant"
    });

    {
        std::string cmd;
        auto result = detector.Detect("Komputer buka notepad", cmd);
        Test("'Komputer buka notepad' -> Command",
             result == Yuki::Voice::WakeWordResult::Command);
        Test("cmd = 'buka notepad'",
             cmd == "buka notepad");
    }

    {
        std::string cmd;
        auto result = detector.Detect("Assistant", cmd);
        Test("'Assistant' -> WakeWordOnly",
             result == Yuki::Voice::WakeWordResult::WakeWordOnly);
    }

    // Default wake words sudah tidak ada
    {
        auto result = detector.Detect("Yuki");
        Test("'Yuki' -> None (custom list)",
             result == Yuki::Voice::WakeWordResult::None);
    }

    // Tambah wake word baru
    detector.AddWakeWord("yuki");
    {
        auto result = detector.Detect("Yuki halo");
        Test("'Yuki halo' -> Command (after AddWakeWord)",
             result == Yuki::Voice::WakeWordResult::Command);
    }

    // Hapus wake word
    detector.RemoveWakeWord("komputer");
    {
        auto result = detector.Detect("Komputer");
        Test("'Komputer' -> None (after RemoveWakeWord)",
             result == Yuki::Voice::WakeWordResult::None);
    }

    detector.ClearWakeWords();
    {
        auto result = detector.Detect("assistant");
        Test("'assistant' -> None (after Clear)",
             result == Yuki::Voice::WakeWordResult::None);
    }
}

int main()
{
    std::cout << "Yuki Wake Word Tests\n";

    TestDefaultWakeWords();
    TestDetectYuki();
    TestPunctuation();
    TestDetectYouKey();
    TestDetectYoureKey();
    TestDetectYourKey();
    TestDetectWakeWordInMiddle();
    TestIgnoreNonWake();
    TestCustomWakeWords();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
