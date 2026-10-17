#include "ai/AIBackend.h"
#include "chat/ChatController.h"
#include "config/Config.h"
#include "config/ConfigManager.h"
#include "memory/MemoryStorage.h"

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class EmptyMemoryStorage final : public Yuki::Memory::MemoryStorage
{
public:
    bool Save(const Yuki::Memory::MemoryEntry&) override { return true; }
    std::optional<Yuki::Memory::MemoryEntry> Load(const std::string&) override
    {
        return std::nullopt;
    }
    bool Remove(const std::string&) override { return false; }
    bool Clear() override { return true; }
    std::vector<Yuki::Memory::MemoryEntry> LoadAll() override { return {}; }
};

class CapturingBackend final : public Yuki::AI::AIBackend
{
public:
    std::string prompt;
    std::string Generate(const std::string& value) override
    {
        prompt = value;
        return "Siap.";
    }
};

int main()
{
    int failures = 0;
    auto check = [&failures](const char* name, bool condition)
    {
        std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
        if (!condition) ++failures;
    };

    Yuki::Config::ConfigManager configManager("config/config.json");
    check("runtime configuration loads", configManager.Load());
    const Yuki::Config::Config& config = configManager.Get();
    auto storage = std::make_unique<EmptyMemoryStorage>();
    Yuki::Memory::MemoryManager memories(std::move(storage));
    auto backend = std::make_unique<CapturingBackend>();
    auto* captured = backend.get();
    Yuki::Chat::ChatController chat(std::move(backend), memories, &config);

    chat.SetMode(Yuki::Chat::ConversationMode::Curhat);
    chat.SendMessage("Hai Yuki, apa kabar?");
    const std::string firstCurhatPrompt = captured->prompt;
    check("Curhat mode uses the Yukihana persona",
          firstCurhatPrompt.find("Kamu Yukihana") != std::string::npos &&
          firstCurhatPrompt.find("kata aku dan kamu") != std::string::npos);
    check("Perintah instructions are absent from the AI prompt",
          firstCurhatPrompt.find("MODE PERINTAH:") == std::string::npos &&
          firstCurhatPrompt.find("MODE CURHAT:") != std::string::npos);
    check("persona identifies Yukihana as Ramzy's AI girlfriend",
          firstCurhatPrompt.find("pacar AI Ramzy") != std::string::npos);
    check("persona includes the user-provided Informatics detail",
          firstCurhatPrompt.find("mahasiswa Informatika") != std::string::npos);
    check("persona welcomes the sayang nickname",
          firstCurhatPrompt.find("senang dipanggil sayang") != std::string::npos &&
          firstCurhatPrompt.find("jangan bilang kamu tidak suka dipanggil sayang") != std::string::npos);
    check("runtime config includes Ramzy's Informatics profile",
          config.persona_prompt.find("mahasiswa Informatika") != std::string::npos);
    check("persona bans action narration",
          firstCurhatPrompt.find("tanpa narasi roleplay atau aksi bertanda bintang") != std::string::npos);
    check("legacy contradictory persona is absent",
          firstCurhatPrompt.find("teman hidup user") == std::string::npos &&
          firstCurhatPrompt.find("selalu terdengar tersenyum") == std::string::npos);

    chat.SendMessage("Aku lagi kepikiran pekerjaan hari ini.");
    const std::string curhatPrompt = captured->prompt;
    check("Curhat mode keeps the same persona base",
          curhatPrompt.find("Kamu Yukihana") != std::string::npos &&
          curhatPrompt.find("kata aku dan kamu") != std::string::npos);
    check("Curhat instructions are separate and mode-specific",
          curhatPrompt.find("MODE CURHAT:") != std::string::npos &&
          curhatPrompt.find("MODE PERINTAH:") == std::string::npos);

    return failures == 0 ? 0 : 1;
}
