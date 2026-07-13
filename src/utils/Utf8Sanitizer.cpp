#include "Utf8Sanitizer.h"

namespace Yuki::Utils {

bool Utf8Sanitizer::IsContinuationByte(unsigned char c) noexcept
{
    return (c & 0xC0) == 0x80;
}

unsigned char Utf8Sanitizer::GetExpectedSequenceLength(unsigned char lead) noexcept
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0; // invalid lead byte
}

bool Utf8Sanitizer::IsValidUtf8(std::string_view input) noexcept
{
    size_t i = 0;
    while (i < input.size())
    {
        unsigned char lead = static_cast<unsigned char>(input[i]);
        unsigned char len = GetExpectedSequenceLength(lead);
        if (len == 0) return false;
        if (i + len > input.size()) return false;

        // Check overlong sequences
        if (len == 2 && lead < 0xC2) return false;
        if (len == 3 && lead == 0xE0 && (static_cast<unsigned char>(input[i+1]) < 0xA0)) return false;
        if (len == 4 && lead == 0xF0 && (static_cast<unsigned char>(input[i+1]) < 0x90)) return false;
        if (len == 4 && lead == 0xF4 && (static_cast<unsigned char>(input[i+1]) > 0x8F)) return false;
        if (len == 4 && lead > 0xF4) return false;

        // Check surrogate halves (U+D800-U+DFFF)
        if (len == 3 && lead == 0xED && (static_cast<unsigned char>(input[i+1]) >= 0xA0)) return false;

        for (unsigned char j = 1; j < len; ++j)
        {
            if (!IsContinuationByte(static_cast<unsigned char>(input[i + j])))
                return false;
        }

        i += len;
    }
    return true;
}

std::string Utf8Sanitizer::Sanitize(std::string_view input) noexcept
{
    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size())
    {
        unsigned char lead = static_cast<unsigned char>(input[i]);
        unsigned char len = GetExpectedSequenceLength(lead);

        if (len == 0 || i + len > input.size())
        {
            // Invalid lead byte or truncated — replace with space
            result += ' ';
            ++i;
            continue;
        }

        // Check overlong
        bool overlong = false;
        if (len == 2 && lead < 0xC2) overlong = true;
        if (len == 3 && lead == 0xE0 && (static_cast<unsigned char>(input[i+1]) < 0xA0)) overlong = true;
        if (len == 4 && lead == 0xF0 && (static_cast<unsigned char>(input[i+1]) < 0x90)) overlong = true;
        if (len == 4 && lead == 0xF4 && (static_cast<unsigned char>(input[i+1]) > 0x8F)) overlong = true;
        if (len == 4 && lead > 0xF4) overlong = true;

        // Check surrogate halves
        bool surrogate = (len == 3 && lead == 0xED && (static_cast<unsigned char>(input[i+1]) >= 0xA0));

        if (overlong || surrogate)
        {
            result += ' ';
            ++i;
            continue;
        }

        // Check continuation bytes
        bool valid = true;
        for (unsigned char j = 1; j < len; ++j)
        {
            if (!IsContinuationByte(static_cast<unsigned char>(input[i + j])))
            {
                valid = false;
                break;
            }
        }

        if (!valid)
        {
            result += ' ';
            ++i;
            continue;
        }

        // Valid — append all bytes
        for (unsigned char j = 0; j < len; ++j)
        {
            result += input[i + j];
        }
        i += len;
    }

    return result;
}

std::string Utf8Sanitizer::SafeTruncateUtf8(
    std::string_view input, size_t maxBytes) noexcept
{
    if (input.size() <= maxBytes || maxBytes == 0)
        return std::string(input.substr(0, maxBytes));

    size_t pos = 0;

    while (pos < input.size())
    {
        unsigned char lead = static_cast<unsigned char>(input[pos]);
        unsigned char len = GetExpectedSequenceLength(lead);
        if (len == 0) len = 1;

        if (pos + len > maxBytes)
            break;

        pos += len;
    }

    return std::string(input.substr(0, pos));
}

} // namespace Yuki::Utils
