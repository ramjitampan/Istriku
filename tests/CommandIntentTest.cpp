#include "system/IntentDetector.h"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    Yuki::System::IntentDetector detector;
    struct Case
    {
        const char* text;
        Yuki::System::CommandType type;
        const char* query;
        bool confirmation;
    };

    const std::vector<Case> cases = {
        {"sleep", Yuki::System::CommandType::Sleep, "", true},
        {"shutdown laptop", Yuki::System::CommandType::Shutdown, "", true},
        {"lock laptop", Yuki::System::CommandType::Lock, "", false},
        {"buka powershell", Yuki::System::CommandType::OpenPowerShell, "", false},
        {"buka informasi sistem", Yuki::System::CommandType::SystemInfo, "", false},
        {"tanggal berapa", Yuki::System::CommandType::DateTime, "", false},
        {"buka whatsapp", Yuki::System::CommandType::OpenWhatsApp, "", false},
        {"buka vscode", Yuki::System::CommandType::OpenVSCode, "", false},
        {"buka folder", Yuki::System::CommandType::OpenExplorer, "", false},
        {"buka microsoft word", Yuki::System::CommandType::OpenWord, "", false},
        {"buka chrome", Yuki::System::CommandType::OpenChrome, "", false},
        {"buka chatgpt", Yuki::System::CommandType::OpenChromeSite, "chatgpt", false},
        {"buka youtube music", Yuki::System::CommandType::OpenYoutubeMusic, "", false},
        {"buka facebook", Yuki::System::CommandType::OpenChromeSite, "facebook", false},
        {"buka tiktok", Yuki::System::CommandType::OpenChromeSite, "tiktok", false},
        {"buka instagram", Yuki::System::CommandType::OpenChromeSite, "instagram", false},
        {"buka youtube", Yuki::System::CommandType::OpenChromeSite, "youtube", false},
        {"buka claude", Yuki::System::CommandType::OpenChromeSite, "claude", false},
        {"buka github", Yuki::System::CommandType::OpenGithub, "", false},
        {"naikkan volume", Yuki::System::CommandType::Volume, "", false},
        {"naikkan brightness", Yuki::System::CommandType::Brightness, "", false},
        {"tidur sayang", Yuki::System::CommandType::Sleep, "", true}
    };

    int failures = 0;
    for (const auto& test : cases)
    {
        const auto result = detector.Detect(test.text);
        const bool passed = result.type == test.type &&
            result.query == test.query &&
            result.requiresConfirmation == test.confirmation;
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.text << '\n';
        failures += passed ? 0 : 1;
    }

    std::cout << cases.size() - failures << '/' << cases.size()
              << " command intent cases passed.\n";
    return failures == 0 ? 0 : 1;
}
