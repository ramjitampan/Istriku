#include "../src/utils/TextNormalizer.h"

#include <iostream>
#include <string>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Utils::TextNormalizer;

int main()
{
    std::cout << "NormalizeRecognition Tests\n";

    std::cout << "\n=== Basic normalization ===\n\n";

    Test("lowercase",
         TextNormalizer::NormalizeRecognition("HELLO World") == "hello world");
    Test("trim spaces",
         TextNormalizer::NormalizeRecognition("  hello  ") == "hello");
    Test("collapse spaces",
         TextNormalizer::NormalizeRecognition("hello   world") == "hello world");
    Test("remove punctuation",
         TextNormalizer::NormalizeRecognition("hello, world!") == "hello world");
    Test("remove control chars",
         TextNormalizer::NormalizeRecognition(std::string("hello\x00world", 11)) == "helloworld");
    Test("empty returns empty",
         TextNormalizer::NormalizeRecognition("").empty());
    Test("only spaces returns empty",
         TextNormalizer::NormalizeRecognition("   ").empty());

    std::cout << "\n=== Kata normalization ===\n\n";

    Test("turun kan -> turunkan",
         TextNormalizer::NormalizeRecognition("turun kan") == "turunkan");
    Test("naik kan -> naikkan",
         TextNormalizer::NormalizeRecognition("naik kan") == "naikkan");
    Test("hidup kan -> hidupkan",
         TextNormalizer::NormalizeRecognition("hidup kan") == "hidupkan");
    Test("mati kan -> matikan",
         TextNormalizer::NormalizeRecognition("mati kan") == "matikan");
    Test("nyala kan -> nyalakan",
         TextNormalizer::NormalizeRecognition("nyala kan") == "nyalakan");
    Test("kunci kan -> kuncikan",
         TextNormalizer::NormalizeRecognition("kunci kan") == "kuncikan");
    Test("terang kan -> terangkan",
         TextNormalizer::NormalizeRecognition("terang kan") == "terangkan");
    Test("redup kan -> redupkan",
         TextNormalizer::NormalizeRecognition("redup kan") == "redupkan");
    Test("gelap kan -> gelapkan",
         TextNormalizer::NormalizeRecognition("gelap kan") == "gelapkan");
    Test("aktif kan -> aktifkan",
         TextNormalizer::NormalizeRecognition("aktif kan") == "aktifkan");

    std::cout << "\n=== Compound word normalization ===\n\n";

    Test("v s code -> vscode",
         TextNormalizer::NormalizeRecognition("v s code") == "vscode");
    Test("vs code -> vscode",
         TextNormalizer::NormalizeRecognition("vs code") == "vscode");
    Test("visual studio code -> vscode",
         TextNormalizer::NormalizeRecognition("visual studio code") == "vscode");
    Test("youtube musik -> youtube music",
         TextNormalizer::NormalizeRecognition("youtube musik") == "youtube music");
    Test("wi fi -> wifi",
         TextNormalizer::NormalizeRecognition("wi fi") == "wifi");
    Test("foto shop -> photoshop",
         TextNormalizer::NormalizeRecognition("foto shop") == "photoshop");
    Test("face book -> facebook",
         TextNormalizer::NormalizeRecognition("face book") == "facebook");
    Test("screen shot -> screenshot",
         TextNormalizer::NormalizeRecognition("screen shot") == "screenshot");

    std::cout << "\n=== Wake word normalization ===\n\n";

    Test("you're key -> yuki",
         TextNormalizer::NormalizeRecognition("you're key") == "yuki");
    Test("your key -> yuki",
         TextNormalizer::NormalizeRecognition("your key") == "yuki");
    Test("u key -> yuki",
         TextNormalizer::NormalizeRecognition("u key") == "yuki");
    Test("you kay -> yuki",
         TextNormalizer::NormalizeRecognition("you kay") == "yuki");
    Test("you key -> yuki",
         TextNormalizer::NormalizeRecognition("you key") == "yuki");
    Test("yuuki -> yuki",
         TextNormalizer::NormalizeRecognition("yuuki") == "yuki");
    Test("yukki -> yuki",
         TextNormalizer::NormalizeRecognition("yukki") == "yuki");

    std::cout << "\n=== Indonesian number normalization ===\n\n";

    Test("seratus -> 100",
         TextNormalizer::NormalizeRecognition("seratus") == "100");
    Test("sepuluh -> 10",
         TextNormalizer::NormalizeRecognition("sepuluh") == "10");
    Test("sebelas -> 11",
         TextNormalizer::NormalizeRecognition("sebelas") == "11");
    Test("nol -> 0",
         TextNormalizer::NormalizeRecognition("nol") == "0");

    std::cout << "\n=== Word-boundary regression ===\n\n";

    Test("kunci kantor tetap kunci kantor (regression)",
         TextNormalizer::NormalizeRecognition("kunci kantor") == "kunci kantor");
    Test("kunci kan pintu -> kuncikan pintu (merge masih jalan)",
         TextNormalizer::NormalizeRecognition("kunci kan pintu") == "kuncikan pintu");
    Test("kunci kan saja -> kuncikan saja",
         TextNormalizer::NormalizeRecognition("kunci kan saja") == "kuncikan saja");
    Test("naik kan naik -> naikkan naik (merge pertama doang)",
         TextNormalizer::NormalizeRecognition("naik kan naik") == "naikkan naik");
    Test("terang kan kancah -> terangkan kancah (space after = valid merge)",
         TextNormalizer::NormalizeRecognition("terang kan kancah") == "terangkan kancah");
    Test("turun kan kantor -> turunkan kantor (space after = valid merge)",
         TextNormalizer::NormalizeRecognition("turun kan kantor") == "turunkan kantor");
    Test("gelap kan kanguru -> gelapkan kanguru (space after = valid merge)",
         TextNormalizer::NormalizeRecognition("gelap kan kanguru") == "gelapkan kanguru");
    Test("redup kan kanvas -> redupkan kanvas (space after = valid merge)",
         TextNormalizer::NormalizeRecognition("redup kan kanvas") == "redupkan kanvas");

    std::cout << "\n=== Single-word boundary regression ===\n\n";

    Test("nyenyak -> tidur (still works)",
         TextNormalizer::NormalizeRecognition("nyenyak") == "tidur");
    Test("nyenyak sekali -> tidur sekali",
         TextNormalizer::NormalizeRecognition("nyenyak sekali") == "tidur sekali");
    Test("seratus -> 100 (number still works)",
         TextNormalizer::NormalizeRecognition("seratus") == "100");
    Test("sepuluh ribu -> 10 ribu",
         TextNormalizer::NormalizeRecognition("sepuluh ribu") == "10 ribu");
    Test("you're keyboard -> you're keyboard (no false wake word merge)",
         TextNormalizer::NormalizeRecognition("you're keyboard") == "you're keyboard");

    std::cout << "\n=== Full pipeline ===\n\n";

    Test("tolong turun kan brightness -> tolong turunkan brightness",
         TextNormalizer::NormalizeRecognition("tolong turun kan brightness")
         == "tolong turunkan brightness");
    Test("buka v s code -> buka vscode",
         TextNormalizer::NormalizeRecognition("buka v s code")
         == "buka vscode");
    Test("  HELLO   WORLD  -> hello world",
         TextNormalizer::NormalizeRecognition("  HELLO   WORLD  ")
         == "hello world");

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
