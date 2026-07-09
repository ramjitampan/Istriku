#pragma once

#include "MemoryStorage.h"
#include "MemoryEntry.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>


struct sqlite3;
struct sqlite3_stmt;

namespace Yuki::Memory {

// Custom deleter so the sqlite3 connection can be owned by a
// std::unique_ptr instead of a raw pointer (no manual new/delete).
struct Sqlite3Deleter {
    void operator()(sqlite3* db) const noexcept;
};

// SQLiteMemoryStorage
// -----------------------------------------------------------------------
// Concrete MemoryStorage implementation backed by a local SQLite database.
// Single responsibility: persist/retrieve MemoryEntry rows.
// Runtime failures are reported via Logger and via return values
// (bool / std::optional / empty vector) — never via exceptions.
// -----------------------------------------------------------------------
class SQLiteMemoryStorage final : public MemoryStorage {
public:
    explicit SQLiteMemoryStorage(std::string databasePath = "memory.db");
    ~SQLiteMemoryStorage() override;

    SQLiteMemoryStorage(const SQLiteMemoryStorage&) = delete;
    SQLiteMemoryStorage& operator=(const SQLiteMemoryStorage&) = delete;

    SQLiteMemoryStorage(SQLiteMemoryStorage&&) noexcept = default;
    SQLiteMemoryStorage& operator=(SQLiteMemoryStorage&&) noexcept = default;

    bool Save(const MemoryEntry& entry) override;
    std::optional<MemoryEntry> Load(const std::string& key) override;
    bool Remove(const std::string& key) override;
    bool Clear() override;
    std::vector<MemoryEntry> LoadAll() override;

private:
    bool Open();
    void Close();
    bool CreateTableIfNeeded();

    bool IsOpen() const noexcept;
    bool PrepareStatement(const char* sql, sqlite3_stmt** outStmt) const;
    bool StepExpectDone(sqlite3_stmt* stmt, const std::string& context) const;
    bool ExecuteNonQuery(const char* sql) const;

    std::string m_databasePath;
    std::unique_ptr<sqlite3, Sqlite3Deleter> m_db;
};

} // namespace Yuki::Memory