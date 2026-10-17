#include "ai/AIBackend.h"
#include "chat/ChatController.h"
#include "chat/CommandRouter.h"
#include "config/Config.h"
#include "memory/MemoryStorage.h"
#include "system/IntentDetector.h"
#include "system/WindowsController.h"

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class EmptyStorage final : public Yuki::Memory::MemoryStorage
{
public:
    bool Save(const Yuki::Memory::MemoryEntry&) override { return true; }
    std::optional<Yuki::Memory::MemoryEntry> Load(const std::string&) override { return std::nullopt; }
    bool Remove(const std::string&) override { return false; }
    bool Clear() override { return true; }
    std::vector<Yuki::Memory::MemoryEntry> LoadAll() override { return {}; }
};

class CountingBackend final : public Yuki::AI::AIBackend
{
public:
    int calls = 0;
    std::string Generate(const std::string&) override
    {
        ++calls;
        return "Respons Curhat.";
    }
};

int main()
{
    int failures = 0;
    auto check = [&failures](const char* name, bool value)
    {
        std::cout << (value ? "[PASS] " : "[FAIL] ") << name << '\n';
        if (!value) ++failures;
    };

    Yuki::Config::Config config;
    auto storage = std::make_unique<EmptyStorage>();
    Yuki::Memory::MemoryManager memory(std::move(storage));
    auto backend = std::make_unique<CountingBackend>();
    auto* backendProbe = backend.get();
    Yuki::Chat::ChatController chat(std::move(backend), memory, &config);
    Yuki::System::WindowsController windows;
    Yuki::System::IntentDetector detector;
    Yuki::Chat::CommandRouter router(chat, windows, detector, config);

    check("startup mode is Perintah", router.GetMode() == Yuki::Chat::ConversationMode::Perintah);
    const auto unknown = router.HandleText("contoh kalimat yang tidak dikenal");
    check("unknown Perintah input gets a short local response",
          unknown.find("tidak dikenali") != std::string::npos);
    check("unknown Perintah input does not call AI", backendProbe->calls == 0);

    check("/curhat changes the mode", router.HandleText("/curhat").find("Curhat") != std::string::npos &&
          router.GetMode() == Yuki::Chat::ConversationMode::Curhat);
    check("Curhat greeting calls AI", router.HandleText("Hai Yuki") == "Respons Curhat." &&
          backendProbe->calls == 1);
    check("Curhat source-code wording does not become a Windows intent",
          router.HandleText("Aku sedang membaca source code VS Code") == "Respons Curhat." &&
          backendProbe->calls == 2);
    check("/perintah changes the mode", router.HandleText("/perintah").find("Perintah") != std::string::npos &&
          router.GetMode() == Yuki::Chat::ConversationMode::Perintah);
    check("/mode reports the active mode", router.HandleText("/mode") == "Mode aktif: Perintah.");

    const auto risky = router.HandleText("shutdown laptop");
    check("risky action requests confirmation", risky.find("yakin") != std::string::npos);
    check("risky action rejection cancels it", router.HandleText("tidak") == "Perintah dibatalkan.");
    check("confirmation routing never calls AI", backendProbe->calls == 2);

    const auto notepadIntent = detector.Detect("buka notepad");
    check("Notepad is a recognized Windows intent",
          notepadIntent.type == Yuki::System::CommandType::OpenNotepad);
    const auto opened = router.HandleText("buka notepad");
    check("Perintah mode routes Notepad to WindowsController",
          opened.find("Notepad") != std::string::npos);
    check("Windows command path never calls AI", backendProbe->calls == 2);

    check("Curhat mode can be re-entered explicitly",
          router.HandleText("/curhat").find("Curhat") != std::string::npos &&
          router.GetMode() == Yuki::Chat::ConversationMode::Curhat);
    check("mode switch commands do not invoke the AI", backendProbe->calls == 2);

    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
