#include "StringFuzzy.h"

#include <algorithm>
#include <vector>

namespace Yuki::Utils {

size_t LevenshteinDistance(
    std::string_view a,
    std::string_view b) noexcept
{
    const size_t m = a.size();
    const size_t n = b.size();

    if (m == 0) return n;
    if (n == 0) return m;

    // Use two-row optimization — only need O(min(m,n)) memory
    const size_t cols = n + 1;
    std::vector<size_t> prev(cols);
    std::vector<size_t> curr(cols);

    for (size_t j = 0; j < cols; ++j)
        prev[j] = j;

    for (size_t i = 1; i <= m; ++i)
    {
        curr[0] = i;
        for (size_t j = 1; j < cols; ++j)
        {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j] + 1,       // deletion
                curr[j - 1] + 1,   // insertion
                prev[j - 1] + cost // substitution
            });
        }
        prev.swap(curr);
    }

    return prev[n];
}

} // namespace Yuki::Utils
