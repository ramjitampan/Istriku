#include "memory/MemoryExtractor.h"

#include <iostream>

using namespace Yuki::Memory;

static void TestCase(
    const std::string& label,
    const std::string& message)
{
    MemoryExtractor extractor;

    std::cout << label << '\n';
    std::cout << "  Input: \"" << message << "\"\n";

    auto memory = extractor.Extract(message);

    if (!memory)
    {
        std::cout << "  (none)\n\n";
        return;
    }

    std::cout << "  " << memory->key << " = " << memory->value
              << "  [p=" << memory->metadata.priority
              << " c=" << memory->metadata.confidence << "]\n\n";
}

int main()
{
    std::cout << "===== ExtractorTest: Single-fact Extractors =====\n\n";

    // ---- Nama ----
    TestCase("1. namaku",
        "Namaku Ramzy");

    TestCase("2. nama saya",
        "Nama saya Ramzy");

    TestCase("3. Full name",
        "Namaku Ramzy Junfaris Hamonangan");

    // ---- Kota ----
    TestCase("4. Multi-word city",
        "Aku tinggal di Binjai Sumatera Utara");

    // ---- Universitas ----
    TestCase("5. Multi-word university",
        "Aku kuliah di Universitas Negeri Padang");

    // ---- Favorit ----
    TestCase("6. aku suka",
        "Aku suka C++");

    // ---- English patterns ----
    TestCase("7. English: my name is",
        "My name is Ramzy");

    TestCase("8. English: i live in",
        "I live in New York");

    TestCase("9. English: i like",
        "I like programming in C++");

    // ---- No match ----
    TestCase("10. No match",
        "Halo apa kabar");

    TestCase("11. Standalone verb (should extract nothing)",
        "Aku berlari");

    std::cout << "===== All tests completed =====\n";

    return 0;
}
