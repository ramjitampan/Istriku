#pragma once

#include "MemoryEntry.h"

#include <optional>
#include <string>
#include <vector>

namespace Yuki::Memory
{
class MemoryStorage
{
public:
    virtual ~MemoryStorage() = default;

    //----------------------------------------------------------
    // Menyimpan atau memperbarui sebuah memory.
    //
    // Return:
    // true  -> berhasil
    // false -> gagal
    //----------------------------------------------------------
    virtual bool Save(
        const MemoryEntry& entry) = 0;

    //----------------------------------------------------------
    // Mengambil memory berdasarkan key.
    //
    // Return:
    // std::nullopt jika tidak ditemukan.
    //----------------------------------------------------------
    virtual std::optional<MemoryEntry> Load(
        const std::string& key) = 0;

    //----------------------------------------------------------
    // Menghapus memory berdasarkan key.
    //----------------------------------------------------------
    virtual bool Remove(
        const std::string& key) = 0;

    //----------------------------------------------------------
    // Menghapus seluruh memory.
    //----------------------------------------------------------
    virtual bool Clear() = 0;

    //----------------------------------------------------------
    // Mengambil seluruh memory.
    //----------------------------------------------------------
    virtual std::vector<MemoryEntry> LoadAll() = 0;
};

} // namespace Yuki::Memory