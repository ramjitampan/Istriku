#include "../src/utils/Utf8Sanitizer.h"
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

using Yuki::Utils::Utf8Sanitizer;
using Yuki::Utils::TextNormalizer;

static void TestValidUtf8()
{
    std::cout << "\n=== Valid UTF-8 ===\n\n";

    Test("ASCII passes through", Utf8Sanitizer::IsValidUtf8("hello"));
    Test("Empty string is valid", Utf8Sanitizer::IsValidUtf8(""));
    Test("Indonesian chars valid", Utf8Sanitizer::IsValidUtf8("buka vscode"));
    Test("2-byte sequence valid",
         Utf8Sanitizer::IsValidUtf8("\xC3\xA9")); // é
    Test("3-byte sequence valid",
         Utf8Sanitizer::IsValidUtf8("\xE2\x82\xAC")); // €
}

static void TestInvalidUtf8()
{
    std::cout << "\n=== Invalid UTF-8 ===\n\n";

    Test("Invalid lead byte", !Utf8Sanitizer::IsValidUtf8("\xFF\xFF"));
    Test("Missing continuation byte", !Utf8Sanitizer::IsValidUtf8("\xC3"));
    Test("Bad continuation byte", !Utf8Sanitizer::IsValidUtf8("\xC3\x20"));
    Test("Overlong 2-byte", !Utf8Sanitizer::IsValidUtf8("\xC0\x80"));
    Test("Surrogate half", !Utf8Sanitizer::IsValidUtf8("\xED\xA0\x80"));
}

static void TestSafeTruncateUtf8()
{
    std::cout << "\n=== SafeTruncateUtf8 ===\n\n";

    // ASCII-only: truncate at byte boundary
    Test("ASCII truncate exact",
         Utf8Sanitizer::SafeTruncateUtf8("hello", 3) == "hel");

    // Shorter than maxBytes returns full string
    Test("Shorter than max returns full",
         Utf8Sanitizer::SafeTruncateUtf8("hi", 100) == "hi");

    // Empty string
    Test("Empty string",
         Utf8Sanitizer::SafeTruncateUtf8("", 5).empty());

    // maxBytes == 0 returns empty
    Test("Zero max returns empty",
         Utf8Sanitizer::SafeTruncateUtf8("hello", 0).empty());

    // 2-byte UTF-8 (é = C3 A9) truncated before it → should cut before
    {
        // é = U+00E9 = {0xC3, 0xA9}
        std::string input = "a\xC3\xA9";
        // Truncate to 2 bytes: "a" (1) + first byte of é (C3) = 2
        // C3 + A9 = 2 bytes, so truncating to 2 means we include "a" + "C3" but
        // C3 starts a 2-byte sequence that needs A9 too. SafeTruncate should cut
        // at position 1 (just "a"), since C3+A9 would be 3 bytes total
        auto result = Utf8Sanitizer::SafeTruncateUtf8(input, 1);
        Test("Truncate before 2-byte char",
             result == "a");
        Test("Result valid UTF-8",
             Utf8Sanitizer::IsValidUtf8(result));
    }

    // 2-byte UTF-8 truncated exactly at sequence end
    {
        std::string input = "a\xC3\xA9";  // "aé" = 3 bytes
        auto result = Utf8Sanitizer::SafeTruncateUtf8(input, 3);
        Test("Truncate at 2-byte char boundary",
             result == input);
        Test("Result valid UTF-8 at boundary",
             Utf8Sanitizer::IsValidUtf8(result));
    }

    // 3-byte UTF-8 (€ = E2 82 AC) truncated in middle
    {
        std::string input = "a\xE2\x82\xAC";  // "a€" = 4 bytes
        // Truncate to 3 bytes → cut before €, keep "a"
        auto result = Utf8Sanitizer::SafeTruncateUtf8(input, 3);
        Test("Truncate before 3-byte char",
             result == "a" && result.size() == 1);
        Test("Result valid UTF-8 (3-byte)",
             Utf8Sanitizer::IsValidUtf8(result));
    }

    // 4-byte UTF-8 (emoji 😁 = F0 9F 98 81) truncated in middle
    {
        std::string input = "X\xF0\x9F\x98\x81";  // "X😁" = 5 bytes
        // Truncate to 2 bytes → cut after "X" only, since 😁 starts at byte 1
        // and takes 4 bytes, which exceeds maxBytes
        auto result = Utf8Sanitizer::SafeTruncateUtf8(input, 2);
        Test("Truncate before 4-byte emoji",
             result == "X" && result.size() == 1);
        Test("Result valid UTF-8 (4-byte emoji)",
             Utf8Sanitizer::IsValidUtf8(result));
    }

    // Emoji truncated exactly at maxBytes = 1 (right at start of 4-byte sequence)
    {
        std::string input = "\xF0\x9F\x98\x81";  // 😁 alone = 4 bytes
        auto result = Utf8Sanitizer::SafeTruncateUtf8(input, 2);
        Test("Emoji - truncate at 2 (should be empty)",
             result.empty());
        Test("Empty result valid UTF-8",
             Utf8Sanitizer::IsValidUtf8(result));
    }

    // Multi-byte mixed with ASCII, truncated exactly at boundary between chars
    {
        // "aé€😁" = 1 + 2 + 3 + 4 = 10 bytes
        std::string input = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x81";
        // Truncate to 3 bytes → "a" + "é" = 3 bytes
        auto result = Utf8Sanitizer::SafeTruncateUtf8(input, 3);
        std::string expected = "a\xC3\xA9";
        Test("Mixed UTF-8 truncate at boundary",
             result == expected && result.size() == 3);
        Test("Mixed result valid UTF-8",
             Utf8Sanitizer::IsValidUtf8(result));
    }

    // JSON crash regression test: emoji truncated → must be valid UTF-8
    {
        std::string input = "Halo \xF0\x9F\x98\x8A";  // "Halo 😊" with grinning face
        for (size_t limit = 0; limit <= input.size(); ++limit)
        {
            auto result = Utf8Sanitizer::SafeTruncateUtf8(input, limit);
            bool valid = Utf8Sanitizer::IsValidUtf8(result);

            if (!valid)
            {
                std::cout << "[FAIL] JSON regression at limit="
                          << limit << "\n";
                ++testsFailed;
            }
            else
            {
                ++testsPassed;
            }
        }
        Test("JSON regression: all limits produce valid UTF-8",
             true); // individual results already counted above
    }
}

static void TestSanitize()
{
    std::cout << "\n=== Sanitize ===\n\n";

    Test("Valid text unchanged",
         Utf8Sanitizer::Sanitize("hello world") == "hello world");
    Test("Invalid bytes replaced with space",
         Utf8Sanitizer::Sanitize("he\xFFllo").find("he llo") != std::string::npos);
    Test("Empty stays empty",
         Utf8Sanitizer::Sanitize("").empty());
    Test("Mixed valid/invalid",
         Utf8Sanitizer::Sanitize("a\xC3\xA9" "\xFF" "b").find("a\xC3\xA9") != std::string::npos);
}

static void TestTextNormalizer()
{
    std::cout << "\n=== Text Normalizer ===\n\n";

    Test("ToLower works",
         TextNormalizer::ToLower("HELLO World") == "hello world");
    Test("Trim removes spaces",
         TextNormalizer::Trim("  hello  ") == "hello");
    Test("CollapseSpaces",
         TextNormalizer::CollapseSpaces("hello   world  test") == "hello world test");
    Test("RemoveControlChars keeps printable",
         TextNormalizer::RemoveControlChars(std::string("hello\x00world", 11)) == "helloworld");
    Test("RemoveControlChars keeps newline",
         TextNormalizer::RemoveControlChars("hello\nworld").find('\n') != std::string::npos);
    Test("Full normalize pipeline",
         TextNormalizer::Normalize(std::string("  HELLO   WORLD\x00TEST  ", 22)) == "hello worldtest");
    Test("Normalize empty returns empty",
         TextNormalizer::Normalize("").empty());
    Test("Normalize only spaces returns empty",
         TextNormalizer::Normalize("   ").empty());
}

int main()
{
    std::cout << "UTF-8 Sanitizer & Text Normalizer Tests\n";

    TestValidUtf8();
    TestInvalidUtf8();
    TestSanitize();
    TestSafeTruncateUtf8();
    TestTextNormalizer();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
