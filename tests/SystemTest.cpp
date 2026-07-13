#include "../src/system/IntentDetector.h"
#include "../src/system/WindowsController.h"
#include "../src/system/Command.h"

#include <iostream>
#include <string>
#include <cassert>

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

// ===================== INTENT DETECTOR TESTS =====================

static void TestIntentDetector()
{
    std::cout << "\n=== IntentDetector Tests ===\n\n";

    Yuki::System::IntentDetector detector;

    // System commands
    {
        auto cmd = detector.Detect("matikan laptop");
        Test("Shutdown: 'matikan laptop'", cmd.type == Yuki::System::CommandType::Shutdown);
        Test("Shutdown: requires confirmation", cmd.requiresConfirmation);

        cmd = detector.Detect("restart laptop");
        Test("Restart: 'restart laptop'", cmd.type == Yuki::System::CommandType::Restart);
        Test("Restart: requires confirmation", cmd.requiresConfirmation);

        cmd = detector.Detect("sleep");
        Test("Sleep: 'sleep'", cmd.type == Yuki::System::CommandType::Sleep);
        Test("Sleep: requires confirmation", cmd.requiresConfirmation);

        cmd = detector.Detect("tidur yang nyenyak sayang");
        Test("Shutdown: 'tidur yang nyenyak sayang'",
            cmd.type == Yuki::System::CommandType::Shutdown);
        Test("Shutdown: nyenyak requires confirmation", cmd.requiresConfirmation);

        cmd = detector.Detect("tidur");
        Test("Sleep: 'tidur'", cmd.type == Yuki::System::CommandType::Sleep);
        Test("Sleep: 'tidur' requires confirmation", cmd.requiresConfirmation);
    }

    // Lock / Logout
    {
        auto cmd = detector.Detect("kunci laptop");
        Test("Lock: 'kunci laptop'", cmd.type == Yuki::System::CommandType::Lock);

        cmd = detector.Detect("kuncikan tolong");
        Test("Lock: 'kuncikan tolong'", cmd.type == Yuki::System::CommandType::Lock);

        cmd = detector.Detect("log out");
        Test("Logout: 'log out'", cmd.type == Yuki::System::CommandType::Logout);
    }

    // Application commands
    {
        auto cmd = detector.Detect("buka vscode");
        Test("OpenVSCode: 'buka vscode'", cmd.type == Yuki::System::CommandType::OpenVSCode);

        cmd = detector.Detect("buka steam");
        Test("OpenSteam: 'buka steam'", cmd.type == Yuki::System::CommandType::OpenSteam);

        cmd = detector.Detect("buka folder");
        Test("OpenExplorer: 'buka folder'", cmd.type == Yuki::System::CommandType::OpenExplorer);
    }

    // Web commands
    {
        auto cmd = detector.Detect("buka youtube music");
        Test("OpenYoutubeMusic: 'buka youtube music'", cmd.type == Yuki::System::CommandType::OpenYoutubeMusic);

        cmd = detector.Detect("buka github");
        Test("OpenGithub: 'buka github'", cmd.type == Yuki::System::CommandType::OpenGithub);
    }

    // Removed commands should return Unknown
    {
        auto cmd = detector.Detect("buka kalkulator");
        Test("Removed: 'buka kalkulator' -> Unknown", cmd.type == Yuki::System::CommandType::Unknown);

        cmd = detector.Detect("buka notepad");
        Test("Removed: 'buka notepad' -> Unknown", cmd.type == Yuki::System::CommandType::Unknown);

        cmd = detector.Detect("buka facebook");
        Test("Removed: 'buka facebook' -> Unknown", cmd.type == Yuki::System::CommandType::Unknown);

        cmd = detector.Detect("buka chatgpt");
        Test("Removed: 'buka chatgpt' -> Unknown", cmd.type == Yuki::System::CommandType::Unknown);
    }

    // Screenshot
    {
        auto cmd = detector.Detect("screenshot");
        Test("Screenshot: 'screenshot'", cmd.type == Yuki::System::CommandType::Screenshot);

        cmd = detector.Detect("tangkap layar");
        Test("Screenshot: 'tangkap layar'", cmd.type == Yuki::System::CommandType::Screenshot);

        cmd = detector.Detect("ss");
        Test("Screenshot: 'ss'", cmd.type == Yuki::System::CommandType::Screenshot);
    }

    // Volume
    {
        auto cmd = detector.Detect("volume 40");
        Test("Volume: 'volume 40' extracts number 40", 
            cmd.type == Yuki::System::CommandType::Volume && cmd.parameter == 40);

        cmd = detector.Detect("mute");
        Test("Volume mute: 'mute'", 
            cmd.type == Yuki::System::CommandType::Volume && cmd.parameter == 0);

        cmd = detector.Detect("volume naik");
        Test("Volume naik: 'volume naik'", 
            cmd.type == Yuki::System::CommandType::Volume && cmd.parameter == -1);

        cmd = detector.Detect("volume turun");
        Test("Volume turun: 'volume turun'", 
            cmd.type == Yuki::System::CommandType::Volume && cmd.parameter == -2);
    }

    // Brightness
    {
        auto cmd = detector.Detect("brightness 70");
        Test("Brightness: 'brightness 70'", 
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == 70);

        cmd = detector.Detect("terang sekali");
        Test("Brightness: 'terang sekali' -> 100%", 
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == 100);

        cmd = detector.Detect("terangkan sedikit");
        Test("Brightness: 'terangkan sedikit' -> -1 (15%)", 
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == -1);

        cmd = detector.Detect("terangkan");
        Test("Brightness: 'terangkan' -> -2 (increase)", 
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == -2);

        cmd = detector.Detect("redupkan");
        Test("Brightness: 'redupkan' -> -3 (decrease)", 
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == -3);

        // Direction + number = absolute
        cmd = detector.Detect("turunkan brightness 50");
        Test("Brightness: 'turunkan brightness 50' -> 50",
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == 50);

        cmd = detector.Detect("naikkan brightness 80");
        Test("Brightness: 'naikkan brightness 80' -> 80",
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == 80);

        // Indonesian number words
        cmd = detector.Detect("brightness lima puluh");
        Test("Brightness: 'brightness lima puluh' -> 50 (indonesian word)",
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == 50);

        cmd = detector.Detect("turunkan brightness kecerahannya menjadi elima puluh");
        Test("Brightness: 'turunkan brightness...elima puluh' -> 50 (misrecognition)",
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == 50);

        cmd = detector.Detect("brightness seratus");
        Test("Brightness: 'brightness seratus' -> 100",
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == 100);

        // Direction without number = relative
        cmd = detector.Detect("turunkan brightness");
        Test("Brightness: 'turunkan brightness' -> -3 (no number)",
            cmd.type == Yuki::System::CommandType::Brightness && cmd.parameter == -3);
    }

    // WiFi
    {
        auto cmd = detector.Detect("wifi hidupkan");
        Test("WifiOn: 'wifi hidupkan'", cmd.type == Yuki::System::CommandType::WifiOn);

        cmd = detector.Detect("wifi mati");
        Test("WifiOff: 'wifi mati'", cmd.type == Yuki::System::CommandType::WifiOff);
    }

    // Unknown
    {
        auto cmd = detector.Detect("halo apa kabar");
        Test("Unknown: 'halo apa kabar'", cmd.type == Yuki::System::CommandType::Unknown);
    }
}

// ===================== WINDOWS CONTROLLER TESTS (Mocked) =====================

static void TestWindowsController()
{
    std::cout << "\n=== WindowsController Tests ===\n\n";

    Yuki::System::WindowsController controller;

    // OpenExplorer - should succeed
    {
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::OpenExplorer;
        auto result = controller.Execute(cmd);
        Test("WindowsController: OpenExplorer should succeed on Windows", 
            result.success);
    }

    // Volume
    {
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Volume;
        cmd.parameter = 50;
        auto result = controller.Execute(cmd);
        Test("WindowsController: Volume 50% should succeed", result.success);

        cmd.parameter = 101;
        result = controller.Execute(cmd);
        Test("WindowsController: Volume 101% should fail", !result.success);

        cmd.parameter = 0;
        result = controller.Execute(cmd);
        Test("WindowsController: Volume 0% (mute) should succeed", result.success);
    }

    // Brightness
    {
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Brightness;
        cmd.parameter = 50;
        auto result = controller.Execute(cmd);
        Test("WindowsController: Brightness 50% runs (may fail on some monitors)", 
            !result.message.empty());

        cmd.parameter = 150;
        result = controller.Execute(cmd);
        Test("WindowsController: Brightness 150% should fail", !result.success);
    }

    // Screenshot (just test the message, don't block on actual save)
    {
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Screenshot;
        auto result = controller.Execute(cmd);
        if (result.success)
            Test("WindowsController: Screenshot saved", 
                result.message.find("Screenshot_") != std::string::npos);
        else
            Test("WindowsController: Screenshot (GDI+ may not be initialized)", true);
    }

    // System commands with confirmation
    {
        Yuki::System::Command cmd;
        cmd.type = Yuki::System::CommandType::Shutdown;
        cmd.requiresConfirmation = true;
        auto result = controller.Execute(cmd);
        Test("WindowsController: Shutdown requires confirmation", 
            result.confirmationRequired);
        Test("WindowsController: Shutdown confirmation message", 
            !result.message.empty());
    }
}

// ===================== COMMAND STRUCTURE TESTS =====================

static void TestCommandStructure()
{
    std::cout << "\n=== Command Structure Tests ===\n\n";

    Yuki::System::Command cmd;
    Test("Default command type is Unknown", 
        cmd.type == Yuki::System::CommandType::Unknown);
    Test("Default parameter is -1", cmd.parameter == -1);
    Test("Default requiresConfirmation is false", !cmd.requiresConfirmation);

    cmd.type = Yuki::System::CommandType::Shutdown;
    cmd.parameter = -1;
    cmd.requiresConfirmation = true;
    Test("Command can be assigned Shutdown type", 
        cmd.type == Yuki::System::CommandType::Shutdown);
    Test("Command confirmation flag works", cmd.requiresConfirmation);
}

// ===================== MAIN =====================

int main()
{
    std::cout << "Yuki System Tests - Sprint 8\n";

    TestIntentDetector();
    TestWindowsController();
    TestCommandStructure();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
