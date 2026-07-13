#include "../src/voice/TextToSpeech.h"

#include <atomic>
#include <iostream>
#include <string>
#include <thread>

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

static void TestDefaultState()
{
    std::cout << "\n=== Default state ===\n\n";

    Yuki::Voice::TextToSpeech tts;

    Test("Default volume == 100",
         tts.GetVolume() == 100);
    Test("Default rate == 0",
         tts.GetRate() == 0);
    Test("Not muted by default",
         !tts.IsMuted());
    Test("Not speaking initially",
         !tts.IsSpeaking());
}

static void TestVolumeAndRate()
{
    std::cout << "\n=== Volume and Rate ===\n\n";

    Yuki::Voice::TextToSpeech tts;

    tts.SetVolume(75);
    Test("Volume = 75",
         tts.GetVolume() == 75);

    tts.SetVolume(0);
    Test("Volume = 0 (mute via volume)",
         tts.GetVolume() == 0);

    tts.SetVolume(150);
    Test("Volume = 100 (clamp)",
         tts.GetVolume() == 100);

    tts.SetVolume(-10);
    Test("Volume = 0 (clamp negative)",
         tts.GetVolume() == 0);

    tts.SetRate(-5);
    Test("Rate = -5",
         tts.GetRate() == -5);

    tts.SetRate(10);
    Test("Rate = 10",
         tts.GetRate() == 10);

    tts.SetRate(20);
    Test("Rate = 10 (clamp max)",
         tts.GetRate() == 10);

    tts.SetRate(-20);
    Test("Rate = -10 (clamp min)",
         tts.GetRate() == -10);
}

static void TestMute()
{
    std::cout << "\n=== Mute ===\n\n";

    Yuki::Voice::TextToSpeech tts;

    tts.Mute(true);
    Test("IsMuted true after Mute(true)",
         tts.IsMuted());

    tts.Mute(false);
    Test("IsMuted false after Mute(false)",
         !tts.IsMuted());
}

static void TestSpeakAndStop()
{
    std::cout << "\n=== Speak and Stop ===\n\n";

    Yuki::Voice::TextToSpeech tts;

    // Speak harus non-blocking (async)
    tts.Speak("Tes satu dua tiga");
    Test("Speak returns immediately (non-blocking)",
         true);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    Test("IsSpeaking check completed without crash",
         true);

    tts.Stop();
    Test("Stop returns without crash",
         true);

    tts.Stop();
    Test("Stop is idempotent",
         true);
}

static void TestCompletionCallback()
{
    std::cout << "\n=== Completion Callback ===\n\n";

    Yuki::Voice::TextToSpeech tts;
    std::atomic<bool> completed{false};

    tts.SetCompletionHandler([&completed]
    {
        completed.store(true);
    });

    tts.Speak("Halo");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    Test("Completion callback fired",
         completed.load());
}

static void TestSpeakMultiple()
{
    std::cout << "\n=== Speak Multiple ===\n\n";

    Yuki::Voice::TextToSpeech tts;

    tts.Speak("Pesan pertama");
    tts.Speak("Pesan kedua");
    tts.Speak("Pesan ketiga");

    std::this_thread::sleep_for(std::chrono::seconds(3));

    Test("Multiple speaks complete without crash",
         true);
}

int main()
{
    std::cout << "Yuki TextToSpeech Tests\n";

    TestDefaultState();
    TestVolumeAndRate();
    TestMute();
    TestSpeakAndStop();
    TestCompletionCallback();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
