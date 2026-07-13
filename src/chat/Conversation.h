#pragma once

#include <string>
#include <vector>

namespace Yuki::Chat
{

enum class MessageRole
{
    User,
    Assistant
};

struct Message
{
    MessageRole role;
    std::string content;
};

class Conversation
{
public:
    Conversation() = default;
    ~Conversation() = default;

    Conversation(const Conversation&) = default;
    Conversation& operator=(const Conversation&) = default;

    Conversation(Conversation&&) noexcept = default;
    Conversation& operator=(Conversation&&) noexcept = default;

    //==============================================================
    // Menambahkan pesan user
    //==============================================================
    void AddUserMessage(const std::string& message);
    void RemoveLastUserMessage();

    //==============================================================
    // Menambahkan pesan AI
    //==============================================================
    void AddAssistantMessage(const std::string& message);

    //==============================================================
    // Menghapus seluruh riwayat
    //==============================================================
    void Clear();

    //==============================================================
    // Mengambil seluruh history (read-only)
    //==============================================================
    const std::vector<Message>& GetMessages() const noexcept;

    //==============================================================
    // Mengecek apakah conversation kosong
    //==============================================================
    bool Empty() const noexcept;

    //==============================================================
    // Jumlah pesan
    //==============================================================
    std::size_t Size() const noexcept;

private:
    std::vector<Message> m_messages;
};

} // namespace Yuki::Chat