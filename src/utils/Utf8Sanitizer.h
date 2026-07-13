#pragma once

#include <string>
#include <string_view>

namespace Yuki::Utils {

class Utf8Sanitizer
{
public:
    Utf8Sanitizer() = delete;

    static std::string Sanitize(std::string_view input) noexcept;

    static bool IsValidUtf8(std::string_view input) noexcept;

    static std::string SafeTruncateUtf8(
        std::string_view input, size_t maxBytes) noexcept;

private:
    static bool IsContinuationByte(unsigned char c) noexcept;
    static unsigned char GetExpectedSequenceLength(unsigned char lead) noexcept;
};

} // namespace Yuki::Utils
