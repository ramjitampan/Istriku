#include "../src/chat/ChatController.h"
#include "../src/config/Config.h"
#include "../src/system/IntentDetector.h"
#include "../src/system/WindowsController.h"
#include "../src/ai/AIBackend.h"
#include "../src/utils/StringFuzzy.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Mock backend for testing
class MockBackend : public Yuki::AI::AIBackend
{
public:
    std::string Generate(const std::string&) override { return ""; }
};

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Chat::ChatController;
using Yuki::Chat::ConversationMode;

static void TestTriggerPhraseDetection()
{
    std::cout << "\n=== Trigger Phrase Detection ===\n\n";

    std::vector<std::string> triggers = {
        "mau curhat", "mau ngomong", "aku cape", "aku capek",
        "mau cerita", "capek nih"
    };

    Test("Exact match: mau curhat",
         ChatController::IsTriggerPhrase("mau curhat", triggers));

    Test("Match with extra text: aku capek banget",
         ChatController::IsTriggerPhrase("aku capek banget", triggers));

    Test("Match at end: hari ini capek nih",
         ChatController::IsTriggerPhrase("hari ini capek nih", triggers));

    Test("No match: buka vscode",
         !ChatController::IsTriggerPhrase("buka vscode", triggers));

    Test("No match: kunci layar",
         !ChatController::IsTriggerPhrase("kunci layar", triggers));

    Test("No match: naikkan brightness",
         !ChatController::IsTriggerPhrase("naikkan brightness", triggers));

    Test("No match: empty string",
         !ChatController::IsTriggerPhrase("", triggers));

    Test("No match: gibberish",
         !ChatController::IsTriggerPhrase("asdfghjk", triggers));

    Test("Match: mau cerita tentang hari ini",
         ChatController::IsTriggerPhrase("mau cerita tentang hari ini", triggers));

    Test("No trigger in command text",
         !ChatController::IsTriggerPhrase("yuki kunci laptop", triggers));
}

static void TestExitPhraseDetection()
{
    std::cout << "\n=== Exit Phrase Detection ===\n\n";

    std::vector<std::string> exits = {
        "udah dulu", "balik ke perintah", "selesai curhat", "cukup dulu"
    };

    Test("Exact match: udah dulu",
         ChatController::IsExitPhrase("udah dulu", exits));

    Test("Match: udah dulu ya",
         ChatController::IsExitPhrase("udah dulu ya", exits));

    Test("Match: balik ke perintah",
         ChatController::IsExitPhrase("balik ke perintah", exits));

    Test("Match: selesai curhat makasih",
         ChatController::IsExitPhrase("selesai curhat makasih", exits));

    Test("No match: mau cerita lagi",
         !ChatController::IsExitPhrase("mau cerita lagi", exits));

    Test("No match: aku capek",
         !ChatController::IsExitPhrase("aku capek", exits));

    Test("No match: empty",
         !ChatController::IsExitPhrase("", exits));
}

static void TestModeTransition()
{
    std::cout << "\n=== Mode Transition ===\n\n";

    Yuki::Memory::MemoryManager dummyMemMgr(Yuki::Config::MemoryConfig{});
    ChatController chat(std::make_unique<MockBackend>(), dummyMemMgr);

    // Default mode harus Perintah
    Test("Default mode is Perintah",
         chat.GetMode() == ConversationMode::Perintah);

    // Set ke Curhat
    chat.SetMode(ConversationMode::Curhat);
    Test("SetMode Curhat works",
         chat.GetMode() == ConversationMode::Curhat);

    // Set kembali ke Perintah
    chat.SetMode(ConversationMode::Perintah);
    Test("SetMode Perintah works",
         chat.GetMode() == ConversationMode::Perintah);

    // Double set
    chat.SetMode(ConversationMode::Curhat);
    chat.SetMode(ConversationMode::Curhat);
    Test("Double set Curhat stays Curhat",
         chat.GetMode() == ConversationMode::Curhat);
}

static void TestLevenshtein()
{
    std::cout << "\n=== Levenshtein Distance ===\n\n";

    using Yuki::Utils::LevenshteinDistance;

    Test("identical strings distance 0",
         LevenshteinDistance("curhat", "curhat") == 0);
    Test("empty vs non-empty",
         LevenshteinDistance("", "abc") == 3);
    Test("both empty",
         LevenshteinDistance("", "") == 0);
    Test("turhat vs curhat distance 1 (t->c only)",
         LevenshteinDistance("turhat", "curhat") == 1);
    Test("tuhan vs curhat distance 3",
         LevenshteinDistance("tuhan", "curhat") == 3);
    Test("tuhar vs curhat distance 3",
         LevenshteinDistance("tuhar", "curhat") == 3);
    Test("juran vs curhat distance 3",
         LevenshteinDistance("juran", "curhat") == 3);
    Test("cape vs capek distance 1",
         LevenshteinDistance("cape", "capek") == 1);
    Test("curat vs curhat distance 1",
         LevenshteinDistance("curat", "curhat") == 1);
    Test("cerita vs cerita distance 0",
         LevenshteinDistance("cerita", "cerita") == 0);
}

static void TestFuzzyTriggerDetection()
{
    std::cout << "\n=== Fuzzy Trigger Detection ===\n\n";

    std::vector<std::string> triggers = {
        "mau curhat", "mau ngomong", "aku cape", "aku capek",
        "mau cerita", "capek nih"
    };

    // Fuzzy yang seharusnya MATCH
    Test("turhat -> curhat (fuzzy match)",
         ChatController::IsTriggerPhrase("turhat", triggers));
    Test("cape -> capek (fuzzy match)",
         ChatController::IsTriggerPhrase("cape", triggers));
    Test("cape banget -> capek (fuzzy match)",
         ChatController::IsTriggerPhrase("cape banget", triggers));
    Test("curat -> curhat (fuzzy match)",
         ChatController::IsTriggerPhrase("curat", triggers));
    Test("ngomong -> ngomong (exact match)",
         ChatController::IsTriggerPhrase("ngomong", triggers));
    Test("cerita dong -> cerita (exact match)",
         ChatController::IsTriggerPhrase("cerita dong", triggers));

    // Fuzzy yang TIDAK boleh match (terlalu jauh)
    Test("tuhan -> curhat (terlalu jauh, no match)",
         !ChatController::IsTriggerPhrase("tuhan", triggers));
    Test("tuhar -> curhat (terlalu jauh, no match)",
         !ChatController::IsTriggerPhrase("tuhar", triggers));
    Test("juran -> curhat (terlalu jauh, no match)",
         !ChatController::IsTriggerPhrase("juran", triggers));

    // False positive check: kata umum yang panjangnya mirip "curhat"
    Test("bulat != curhat (false positive check)",
         !ChatController::IsTriggerPhrase("bulat", triggers));
    Test("surat != curhat (false positive check)",
         !ChatController::IsTriggerPhrase("surat", triggers));
    Test("kurus != curhat (false positive check)",
         !ChatController::IsTriggerPhrase("kurus", triggers));
    Test("caret != curhat (false positive check)",
         !ChatController::IsTriggerPhrase("caret", triggers));

    // Exact match masih harus jalan
    Test("mau curhat exact match masih jalan",
         ChatController::IsTriggerPhrase("mau curhat", triggers));
    Test("aku capek exact match masih jalan",
         ChatController::IsTriggerPhrase("aku capek", triggers));
}

static void TestFuzzyExitDetection()
{
    std::cout << "\n=== Fuzzy Exit Detection ===\n\n";

    std::vector<std::string> exits = {
        "udah dulu", "balik ke perintah", "selesai curhat", "cukup dulu"
    };

    // Exact match
    Test("udah dulu exact match",
         ChatController::IsExitPhrase("udah dulu", exits));
    Test("selesai curhat exact match",
         ChatController::IsExitPhrase("selesai curhat", exits));

    // Fuzzy match
    Test("selesai curat -> curhat (fuzzy exit)",
         ChatController::IsExitPhrase("selesai curat", exits));
    Test("balik ke perntah -> perintah (fuzzy)",
         ChatController::IsExitPhrase("balik ke perntah", exits));

    // "dulu" exact match dengan exit phrase "udah dulu" → wajar match
    Test("ngobrol dulu contains 'dulu' exit word",
         ChatController::IsExitPhrase("ngobrol dulu", exits));
}

int main()
{
    std::cout << "Mode Transition & Phrase Detection Tests\n";

    TestTriggerPhraseDetection();
    TestExitPhraseDetection();
    TestModeTransition();
    TestLevenshtein();
    TestFuzzyTriggerDetection();
    TestFuzzyExitDetection();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
