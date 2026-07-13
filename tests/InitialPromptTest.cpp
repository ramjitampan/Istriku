#include "../src/config/Config.h"

#include <iostream>
#include <string>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Config::VoiceConfig;

int main()
{
    std::cout << "Initial Prompt Tests\n";

    std::cout << "\n=== Default prompt content ===\n\n";

    VoiceConfig cfg;
    Test("initial_prompt is not empty", !cfg.initial_prompt.empty());

    // Check that the prompt contains key Indonesian command words
    auto& prompt = cfg.initial_prompt;
    Test("Contains 'Yuki'",
         prompt.find("Yuki") != std::string::npos);
    Test("Contains 'Buka VS Code'",
         prompt.find("Buka VS Code") != std::string::npos);
    Test("Contains 'Shutdown'",
         prompt.find("Shutdown") != std::string::npos);
    Test("Contains 'Brightness'",
         prompt.find("Brightness") != std::string::npos);
    Test("Contains 'Volume'",
         prompt.find("Volume") != std::string::npos);
    Test("Contains 'WiFi'",
         prompt.find("WiFi") != std::string::npos);
    Test("Contains 'Bahasa Indonesia'",
         prompt.find("Bahasa Indonesia") != std::string::npos);

    std::cout << "\n=== Prompt is configurable ===\n\n";

    VoiceConfig custom;
    custom.initial_prompt = "Custom prompt test";
    Test("Custom prompt set correctly",
         custom.initial_prompt == "Custom prompt test");

    VoiceConfig empty;
    empty.initial_prompt.clear();
    Test("Empty prompt is allowed",
         empty.initial_prompt.empty());

    std::cout << "\n=== Language config ===\n\n";

    Test("Default language is 'id'",
         cfg.language == "id");

    VoiceConfig langCustom;
    langCustom.language = "en";
    Test("Custom language set correctly",
         langCustom.language == "en");

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
