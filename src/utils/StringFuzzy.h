#pragma once

#include <string>
#include <string_view>

namespace Yuki::Utils {

size_t LevenshteinDistance(
    std::string_view a,
    std::string_view b) noexcept;

} // namespace Yuki::Utils
