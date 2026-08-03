#include "../src/chat/ChatController.h"
#include "../src/config/Config.h"
#include "../src/system/IntentDetector.h"
#include "../src/ai/AIBackend.h"

#include <iostream>
#include <memory>
#include <string>

// Mock backend yang mengembalikan respons berisi tag [CMD:...] halusinasi
class MockBackend : public Yuki::AI::AIBackend
{
public:
    std::string reply;

    std::string Generate(const std::string&) override { return reply; }
};

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Chat::ChatController;

static void TestAIResponseNeverExecutesCommands()
{
    std::cout << "\n=== AI reply dengan [CMD:...] di akhir TIDAK dieksekusi ===\n\n";

    Yuki::Memory::MemoryManager dummyMemMgr(Yuki::Config::MemoryConfig{});
    auto backend = std::make_unique<MockBackend>();
    backend->reply = "Halo, sayang! Kamu baik-baik saja hari ini? [CMD:Volume 38]";
    ChatController chat(std::move(backend), dummyMemMgr);

    std::string result = chat.SendMessage("hallo");

    Test("Tidak ada teks hasil eksekusi command (Volume diatur)",
         result.find("Volume diatur") == std::string::npos);
    Test("Tag [CMD:...] dibuang dari teks tampilan",
         result.find("[CMD:") == std::string::npos);
    Test("Kalimat sapaan tetap utuh",
         result.find("Halo, sayang!") != std::string::npos);
}

static void TestAICommandAtStartIsNotExecuted()
{
    std::cout << "\n=== AI reply diawali [CMD:...] pun TIDAK dieksekusi ===\n\n";

    Yuki::Memory::MemoryManager dummyMemMgr(Yuki::Config::MemoryConfig{});
    auto backend = std::make_unique<MockBackend>();
    backend->reply = "[CMD:VolumeUp] Siap sayang, sudah kubuat lebih nyaman.";
    ChatController chat(std::move(backend), dummyMemMgr);

    std::string result = chat.SendMessage("hallo");

    Test("Tidak ada eksekusi command (tidak ada pesan volume)",
         result.find("Volume diatur") == std::string::npos);
    Test("Tag [CMD:...] dibuang",
         result.find("[CMD:") == std::string::npos);
    Test("Teks natural tetap ada",
         result.find("Siap sayang") != std::string::npos);
}

static void TestRuleBasedVolumeCommand()
{
    std::cout << "\n=== Perintah rule-based (dari ucapan user) tetap terdeteksi ===\n\n";

    Yuki::System::IntentDetector detector;

    auto cmd = detector.Detect("hei yuki naiki volume 10%");
    Test("'hei yuki naiki volume 10%' adalah Volume",
         cmd.type == Yuki::System::CommandType::Volume);
    Test("Naik relatif (VolumeUp, -1)",
         cmd.parameter == -1);

    cmd = detector.Detect("volume 10");
    Test("'volume 10' adalah Volume",
         cmd.type == Yuki::System::CommandType::Volume);
    Test("Parameter absolut adalah 10",
         cmd.parameter == 10);

    cmd = detector.Detect("buka vscode");
    Test("'buka vscode' adalah OpenVSCode",
         cmd.type == Yuki::System::CommandType::OpenVSCode);
}

int main()
{
    std::cout << "No AI Command Execution Tests\n";

    TestAIResponseNeverExecutesCommands();
    TestAICommandAtStartIsNotExecuted();
    TestRuleBasedVolumeCommand();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
