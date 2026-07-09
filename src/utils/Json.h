#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <initializer_list>

#include <nlohmann/json.hpp>

namespace Yuki::Utils
{

//=====================================================================
// JsonValue
//
// Thin RAII wrapper around nlohmann::json.
//
// The rest of the project should only interact with JsonValue and the
// helper functions inside namespace Json.
//=====================================================================

class JsonValue
{
public:

    JsonValue();
    explicit JsonValue(nlohmann::json value);
    ~JsonValue();

    JsonValue(const JsonValue&);
    JsonValue& operator=(const JsonValue&);

    JsonValue(JsonValue&&) noexcept;
    JsonValue& operator=(JsonValue&&) noexcept;

    [[nodiscard]]
    bool IsNull() const noexcept;

    [[nodiscard]]
    bool IsObject() const noexcept;

    [[nodiscard]]
    bool IsArray() const noexcept;

    [[nodiscard]]
    bool IsValid() const noexcept;

    const nlohmann::json& Raw() const noexcept;
    nlohmann::json& Raw() noexcept;

private:

    std::unique_ptr<nlohmann::json> m_data;
};

using JsonObject = JsonValue;
using JsonArray  = JsonValue;

//=====================================================================
// Json helper functions
//=====================================================================

namespace Json
{

[[nodiscard]]
JsonValue Parse(const std::string& text);

[[nodiscard]]
std::string Stringify(const JsonValue& value);

[[nodiscard]]
std::string Prettify(
    const JsonValue& value,
    int indent = 2);

[[nodiscard]]
std::optional<std::string> GetString(
    const JsonValue& object,
    const std::string& key);

[[nodiscard]]
std::optional<int> GetInt(
    const JsonValue& object,
    const std::string& key);

[[nodiscard]]
std::optional<bool> GetBool(
    const JsonValue& object,
    const std::string& key);

[[nodiscard]]
std::optional<double> GetDouble(
    const JsonValue& object,
    const std::string& key);

[[nodiscard]]
std::vector<JsonValue> GetArray(
    const JsonValue& object,
    const std::string& key);

[[nodiscard]]
std::optional<JsonValue> GetObject(
    const JsonValue& object,
    const std::string& key);

[[nodiscard]]
bool HasKey(
    const JsonValue& object,
    const std::string& key);

[[nodiscard]]
JsonValue MakeObject(
    std::initializer_list<
        std::pair<std::string, std::string>> pairs);

void SetString(
    JsonValue& object,
    const std::string& key,
    const std::string& value);

void SetInt(
    JsonValue& object,
    const std::string& key,
    int value);

void SetBool(
    JsonValue& object,
    const std::string& key,
    bool value);

[[nodiscard]]
JsonValue NewObject();

[[nodiscard]]
JsonValue NewArray();

void Push(
    JsonValue& array,
    JsonValue value);

} // namespace Json

} // namespace Yuki::Utils