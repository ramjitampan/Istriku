#include "memory/MemoryRules.h"

#include <iostream>

using namespace Yuki::Memory;

static void TestCase(
    const std::string& label,
    const std::string& message,
    int expected)
{
    MemoryRules rules;
    int result = rules.ShouldRemember(message) ? 1 : 0;
    const char* status = (result == expected) ? "PASS" : "FAIL";

    std::cout
        << status
        << " | " << label
        << " | got " << result
        << " exp " << expected
        << '\n';
}

int main()
{
    std::cout << "===== MemoryRulesTest =====\n\n";

    // ============================================
    // Statements — should return 1 (remember)
    // ============================================
    std::cout << "--- Statements (expect 1) ---\n";

    TestCase("namaku",
        "Namaku Ramzy", 1);

    TestCase("nama saya",
        "Nama saya Ramzy", 1);

    TestCase("aku + capitalized (name)",
        "Aku Ramzy", 1);

    TestCase("saya + capitalized (name)",
        "Saya Ramzy", 1);

    TestCase("full name",
        "Namaku Ramzy Junfaris Hamonangan", 1);

    TestCase("aku suka",
        "Aku suka C++", 1);

    TestCase("saya suka",
        "Saya suka C++", 1);

    TestCase("aku tinggal di",
        "Aku tinggal di Binjai", 1);

    TestCase("saya tinggal di",
        "Saya tinggal di Binjai", 1);

    TestCase("aku berasal dari",
        "Aku berasal dari Binjai", 1);

    TestCase("saya berasal dari",
        "Saya berasal dari Binjai", 1);

    TestCase("aku kuliah di",
        "Aku kuliah di UNP", 1);

    TestCase("saya kuliah di",
        "Saya kuliah di UNP", 1);

    TestCase("asalku",
        "Asalku Binjai", 1);

    TestCase("rumahku di",
        "Rumahku di Binjai", 1);

    TestCase("hobiku",
        "Hobiku membaca buku", 1);

    TestCase("bahasa favoritku",
        "Bahasa favoritku C++", 1);

    TestCase("aku mahasiswa",
        "Aku mahasiswa Universitas Negeri Padang", 1);

    TestCase("compound: name + city + favorite",
        "Namaku Ramzy aku tinggal di Binjai aku suka C++", 1);

    // ============================================
    // Questions — should return 0 (skip)
    // ============================================
    std::cout << "\n--- Questions (expect 0) ---\n";

    TestCase("? at end",
        "Aku suka apa?", 0);

    TestCase("apa at end no ?",
        "Aku suka apa", 0);

    TestCase("siapa at start",
        "Siapa namaku", 0);

    TestCase("dimana",
        "Dimana aku tinggal", 0);

    TestCase("kapan",
        "Kapan aku lahir", 0);

    TestCase("bagaimana",
        "Bagaimana kabarmu", 0);

    TestCase("kenapa",
        "Kenapa aku begini", 0);

    TestCase("jam berapa",
        "Jam berapa sekarang", 0);

    TestCase("ingat",
        "Masih ingat aku", 0);

    TestCase("remember",
        "Remember my name", 0);

    TestCase("di mana (with space)",
        "Di mana rumahku", 0);

    // ============================================
    // Non-fact messages — should return 0
    // ============================================
    std::cout << "\n--- Non-fact (expect 0) ---\n";

    TestCase("greeting",
        "Halo", 0);

    TestCase("random chat",
        "Apa kabar hari ini", 0);

    // NOTE: MemoryRules is a coarse pre-filter.  It lets "aku "
    // through because the keyword exists.  The Extractor's
    // ValidateMatch (case check on the original message) is the
    // fine filter that rejects lowercase-after-aku/saya.
    // So from MemoryRules' perspective, these are "remember" (1)
    // even though nothing ends up stored.
    TestCase("lowercase aku verb (no cap) — coarse filter",
        "aku berlari", 1);

    TestCase("saya lowercase verb — coarse filter",
        "saya bermain bola", 1);

    std::cout << "\n===== All tests completed =====\n";

    return 0;
}
