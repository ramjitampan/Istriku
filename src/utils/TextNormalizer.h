#pragma once

#include <string>
#include <string_view>

namespace Yuki::Utils {

class TextNormalizer
{
public:
    TextNormalizer() = delete;

    static std::string Normalize(std::string_view input) noexcept;

    static std::string NormalizeRecognition(std::string_view input) noexcept;

    static std::string ToLower(std::string_view input) noexcept;

    static std::string Trim(std::string_view input) noexcept;

    static std::string CollapseSpaces(std::string_view input) noexcept;

    static std::string RemoveControlChars(std::string_view input) noexcept;

    static std::string RemovePunctuation(std::string_view input) noexcept;
};

} // namespace Yuki::Utils
