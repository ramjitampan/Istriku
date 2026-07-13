#include "memory/MemoryExtractor.h"

#include <iostream>

using namespace Yuki::Memory;

static void PrintMemories(
    const std::vector<MemoryEntry>& memories)
{
    if (memories.empty())
    {
        std::cout << "(none)\n";
        return;
    }

    for (const auto& m : memories)
    {
        std::cout << "  " << m.key << " = " << m.value
                  << "  [p=" << m.metadata.priority
                  << " c=" << m.metadata.confidence
                  << "]\n";
    }
}

static void TestCase(
    const std::string& label,
    const std::string& message)
{
    MemoryExtractor extractor;

    std::cout << label << '\n';
    std::cout << "  Input: \"" << message << "\"\n";

    auto memories = extractor.Extract(message);

    PrintMemories(memories);
    std::cout << '\n';
}

int main()
{
    std::cout << "===== ExtractorTest: Sequential Parser =====\n\n";

    // ---- Basic single facts ----
    TestCase("1. namaku",
        "Namaku Ramzy");

    TestCase("2. nama saya",
        "Nama saya Ramzy");

    TestCase("3. Aku standalone + capitalized",
        "Aku Ramzy");

    TestCase("4. Saya standalone + capitalized",
        "Saya Ramzy");

    // ---- Multi-word values ----
    TestCase("5. Full name",
        "Namaku Ramzy Junfaris Hamonangan");

    TestCase("6. Multi-word city",
        "Aku tinggal di Binjai Sumatera Utara");

    TestCase("7. Multi-word university",
        "Aku kuliah di Universitas Negeri Padang");

    // ---- Compound sentence (multiple facts) ----
    TestCase("8. Multiple facts in one sentence",
        "Namaku Ramzy aku tinggal di Binjai aku suka belajar C++");

    TestCase("9. Name + city + favorite",
        "Namaku Ramzy Junfaris Hamonangan aku tinggal di Binjai aku suka C++");

    // ---- New patterns ----
    TestCase("10. berasal dari",
        "Aku berasal dari Binjai");

    TestCase("11. bahasa favoritku",
        "Bahasa favoritku C++");

    TestCase("12. asalku",
        "Asalku Binjai");

    TestCase("13. hobiku",
        "Hobiku membaca buku");

    TestCase("14. rumahku di",
        "Rumahku di Binjai");

    TestCase("15. aku mahasiswa",
        "Aku mahasiswa Universitas Negeri Padang");

    // ---- Standalone aku/saya with lowercase verb (should NOT extract) ----
    TestCase("16. Aku lowercase verb (should extract nothing)",
        "Aku berlari");

    TestCase("17. Saya lowercase verb (should extract nothing)",
        "Saya bermain");

    // ---- Questions (should filter via MemoryRules, but Extractor is
    //       a pure pattern matcher — questions can still match here) ----
    TestCase("18. Question with ? at end",
        "Aku suka apa?");

    TestCase("19. Question word at end no ?",
        "Aku suka apa");

    TestCase("20. No match",
        "Halo apa kabar");

    // ---- Edge: overlapping keywords ----
    TestCase("21. aku suka beats aku",
        "Aku suka C++");

    TestCase("22. aku tinggal di beats aku",
        "Aku tinggal di Binjai");

    // ---- English patterns ----
    TestCase("23. English: my name is",
        "My name is Ramzy");

    TestCase("24. English: i live in",
        "I live in New York");

    TestCase("25. English: i like",
        "I like programming in C++");

    // ---- Confidence: hedging words lower confidence ----
    TestCase("26. Certain statement (confidence 1.0)",
        "Aku suka C++");

    TestCase("27. Hedging: aku rasa (confidence 0.55)",
        "Aku rasa aku suka Rust");

    TestCase("28. Hedging: mungkin (confidence 0.55)",
        "Mungkin aku suka Python");

    TestCase("29. Hedging: i think (confidence 0.55)",
        "I think i like Go");

    std::cout << "===== All tests completed =====\n";

    return 0;
}
