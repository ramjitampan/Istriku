#pragma once
#include <string>

namespace Yuki::AI {

// AIBackend
// -----------------------------------------------------------------------
// Pure abstract interface for any AI text-generation backend.
// MainWindow depends only on this interface, never on a concrete
// implementation such as OllamaClient.
// -----------------------------------------------------------------------
class AIBackend {
public:
    virtual ~AIBackend() = default;

    // Sends a prompt to the backend and returns the generated text.
    // Returns an empty string on failure.
    virtual std::string Generate(const std::string& prompt) = 0;
};

} // namespace Yuki::AI