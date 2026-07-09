#include "Json.h"

#include <nlohmann/json.hpp>

namespace Yuki::Utils
{

using NJson = nlohmann::json;

//=====================================================================
// JsonValue
//=====================================================================

JsonValue::JsonValue()
    : m_data(std::make_unique<NJson>())
{
}

JsonValue::JsonValue(NJson value)
    : m_data(std::make_unique<NJson>(std::move(value)))
{
}

JsonValue::~JsonValue() = default;

JsonValue::JsonValue(const JsonValue& other)
    : m_data(std::make_unique<NJson>(*other.m_data))
{
}

JsonValue& JsonValue::operator=(const JsonValue& other)
{
    if (this != &other)
    {
        m_data = std::make_unique<NJson>(*other.m_data);
    }

    return *this;
}

JsonValue::JsonValue(JsonValue&&) noexcept = default;
JsonValue& JsonValue::operator=(JsonValue&&) noexcept = default;

bool JsonValue::IsNull() const noexcept
{
    return m_data->is_null();
}

bool JsonValue::IsObject() const noexcept
{
    return m_data->is_object();
}

bool JsonValue::IsArray() const noexcept
{
    return m_data->is_array();
}

bool JsonValue::IsValid() const noexcept
{
    return !m_data->is_null();
}

const NJson& JsonValue::Raw() const noexcept
{
    return *m_data;
}

NJson& JsonValue::Raw() noexcept
{
    return *m_data;
}

//=====================================================================
// Json
//=====================================================================

namespace Json
{

JsonValue Parse(const std::string& text)
{
    try
    {
        return JsonValue(NJson::parse(text));
    }
    catch (...)
    {
        return JsonValue();
    }
}

std::string Stringify(const JsonValue& value)
{
    return value.Raw().dump();
}

std::string Prettify(
    const JsonValue& value,
    int indent)
{
    return value.Raw().dump(indent);
}

std::optional<std::string> GetString(
    const JsonValue& object,
    const std::string& key)
{
    const auto& json = object.Raw();

    if (!json.is_object() || !json.contains(key))
        return std::nullopt;

    if (!json[key].is_string())
        return std::nullopt;

    return json[key].get<std::string>();
}

std::optional<int> GetInt(
    const JsonValue& object,
    const std::string& key)
{
    const auto& json = object.Raw();

    if (!json.is_object() || !json.contains(key))
        return std::nullopt;

    if (!json[key].is_number_integer())
        return std::nullopt;

    return json[key].get<int>();
}

std::optional<bool> GetBool(
    const JsonValue& object,
    const std::string& key)
{
    const auto& json = object.Raw();

    if (!json.is_object() || !json.contains(key))
        return std::nullopt;

    if (!json[key].is_boolean())
        return std::nullopt;

    return json[key].get<bool>();
}

std::optional<double> GetDouble(
    const JsonValue& object,
    const std::string& key)
{
    const auto& json = object.Raw();

    if (!json.is_object() || !json.contains(key))
        return std::nullopt;

    if (!json[key].is_number())
        return std::nullopt;

    return json[key].get<double>();
}

std::vector<JsonValue> GetArray(
    const JsonValue& object,
    const std::string& key)
{
    std::vector<JsonValue> result;

    const auto& json = object.Raw();

    if (!json.is_object() || !json.contains(key))
        return result;

    if (!json[key].is_array())
        return result;

    result.reserve(json[key].size());

    for (const auto& item : json[key])
    {
        result.emplace_back(item);
    }

    return result;
}

std::optional<JsonValue> GetObject(
    const JsonValue& object,
    const std::string& key)
{
    const auto& json = object.Raw();

    if (!json.is_object() || !json.contains(key))
        return std::nullopt;

    if (!json[key].is_object())
        return std::nullopt;

    return JsonValue(json[key]);
}

bool HasKey(
    const JsonValue& object,
    const std::string& key)
{
    const auto& json = object.Raw();

    return json.is_object() && json.contains(key);
}

JsonValue MakeObject(
    std::initializer_list<
        std::pair<std::string, std::string>> pairs)
{
    NJson json = NJson::object();

    for (const auto& pair : pairs)
    {
        json[pair.first] = pair.second;
    }

    return JsonValue(std::move(json));
}

void SetString(
    JsonValue& object,
    const std::string& key,
    const std::string& value)
{
    object.Raw()[key] = value;
}

void SetInt(
    JsonValue& object,
    const std::string& key,
    int value)
{
    object.Raw()[key] = value;
}

void SetBool(
    JsonValue& object,
    const std::string& key,
    bool value)
{
    object.Raw()[key] = value;
}

JsonValue NewObject()
{
    return JsonValue(NJson::object());
}

JsonValue NewArray()
{
    return JsonValue(NJson::array());
}

void Push(
    JsonValue& array,
    JsonValue value)
{
    array.Raw().push_back(std::move(value.Raw()));
}

} // namespace Json

} // namespace Yuki::Utils