#include "Conversation.h"

namespace Yuki::Chat
{

//==============================================================
// Menambahkan pesan dari user
//==============================================================
void Conversation::AddUserMessage(const std::string& message)
{
    m_messages.push_back(
    {
        MessageRole::User,
        message
    });
}

//==============================================================
// Menambahkan pesan dari AI
//==============================================================
void Conversation::AddAssistantMessage(const std::string& message)
{
    m_messages.push_back(
    {
        MessageRole::Assistant,
        message
    });
}

//==============================================================
// Menghapus seluruh riwayat
//==============================================================
void Conversation::Clear()
{
    m_messages.clear();
}

//==============================================================
// Mengambil seluruh history
//==============================================================
const std::vector<Message>& Conversation::GetMessages() const noexcept
{
    return m_messages;
}

//==============================================================
// Mengecek apakah conversation kosong
//==============================================================
bool Conversation::Empty() const noexcept
{
    return m_messages.empty();
}

//==============================================================
// Jumlah pesan
//==============================================================
std::size_t Conversation::Size() const noexcept
{
    return m_messages.size();
}

} // namespace Yuki::Chat