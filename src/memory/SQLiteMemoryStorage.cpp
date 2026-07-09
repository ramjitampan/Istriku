#include "SQLiteMemoryStorage.h"
#include "utils/Logger.h"

#include <sqlite3.h>

#include <utility>

namespace Yuki::Memory {

namespace {

class StmtGuard {
public:
    StmtGuard() = default;
    ~StmtGuard() { Reset(); }

    StmtGuard(const StmtGuard&) = delete;
    StmtGuard& operator=(const StmtGuard&) = delete;

    StmtGuard(StmtGuard&& other) noexcept
        : m_stmt(std::exchange(other.m_stmt, nullptr)) {
    }

    StmtGuard& operator=(StmtGuard&& other) noexcept {
        if (this != &other) {
            Reset();
            m_stmt = std::exchange(other.m_stmt, nullptr);
        }
        return *this;
    }

    sqlite3_stmt** OutParam() noexcept { return &m_stmt; }
    sqlite3_stmt* Get() const noexcept { return m_stmt; }

private:
    void Reset() noexcept {
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
            m_stmt = nullptr;
        }
    }

    sqlite3_stmt* m_stmt = nullptr;
};

// Logs the last sqlite3 error message for the given connection.
void LogSqliteError(sqlite3* db, const std::string& context) {
    const std::string message = db ? sqlite3_errmsg(db) : "no open database connection";
    Logger::Error("[SQLiteMemoryStorage] " + context + ": " + message);
}

} // anonymous namespace

// ===========================================================================
// Sqlite3Deleter
// ===========================================================================

void Sqlite3Deleter::operator()(sqlite3* db) const noexcept {
    if (db) {
        sqlite3_close_v2(db);
    }
}

// ===========================================================================
// Construction / destruction
// ===========================================================================

SQLiteMemoryStorage::SQLiteMemoryStorage(std::string databasePath)
    : m_databasePath(std::move(databasePath)) {
    if (!Open()) {
        return;
    }
    if (!CreateTableIfNeeded()) {
        Close();
    }
}

SQLiteMemoryStorage::~SQLiteMemoryStorage() {
    Close();
}

// ===========================================================================
// Private helpers
// ===========================================================================

bool SQLiteMemoryStorage::IsOpen() const noexcept {
    return m_db != nullptr;
}

bool SQLiteMemoryStorage::Open() {
    sqlite3* rawDb = nullptr;
    const int rc = sqlite3_open(m_databasePath.c_str(), &rawDb);

    // sqlite3_open may allocate a handle even on failure; wrap it
    // immediately so it is released either way, never leaked.
    m_db.reset(rawDb);

    if (rc != SQLITE_OK) {
        LogSqliteError(m_db.get(), "failed to open database '" + m_databasePath + "'");
        m_db.reset();
        return false;
    }

    Logger::Info("[SQLiteMemoryStorage] database opened: " + m_databasePath);
    return true;
}

void SQLiteMemoryStorage::Close() {
    if (m_db) {
        m_db.reset();
        Logger::Info("[SQLiteMemoryStorage] database closed: " + m_databasePath);
    }
}

bool SQLiteMemoryStorage::CreateTableIfNeeded() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS memories "
        "(key TEXT PRIMARY KEY, value TEXT NOT NULL);";

    if (!ExecuteNonQuery(sql)) {
        return false;
    }

    Logger::Info("[SQLiteMemoryStorage] table created (or already existed): memories");
    return true;
}

bool SQLiteMemoryStorage::PrepareStatement(const char* sql, sqlite3_stmt** outStmt) const {
    if (!IsOpen()) {
        Logger::Error("[SQLiteMemoryStorage] cannot prepare statement: database is not open");
        return false;
    }

    if (sqlite3_prepare_v2(m_db.get(), sql, -1, outStmt, nullptr) != SQLITE_OK) {
        LogSqliteError(m_db.get(), "failed to prepare statement");
        return false;
    }

    return true;
}

bool SQLiteMemoryStorage::StepExpectDone(sqlite3_stmt* stmt, const std::string& context) const {
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LogSqliteError(m_db.get(), context);
        return false;
    }
    return true;
}

bool SQLiteMemoryStorage::ExecuteNonQuery(const char* sql) const {
    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return false;
    }
    return StepExpectDone(stmt.Get(), "failed to execute statement");
}

// ===========================================================================
// Public API
// ===========================================================================

bool SQLiteMemoryStorage::Save(const MemoryEntry& entry) {
    const char* sql =
        "INSERT INTO memories(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return false;
    }

    sqlite3_bind_text(stmt.Get(), 1, entry.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.Get(), 2, entry.value.c_str(), -1, SQLITE_TRANSIENT);

    return StepExpectDone(stmt.Get(), "Save: failed to execute statement for key '" + entry.key + "'");
}

std::optional<MemoryEntry> SQLiteMemoryStorage::Load(const std::string& key) {
    const char* sql = "SELECT value FROM memories WHERE key = ?;";

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt.Get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt.Get());

    if (rc == SQLITE_ROW) {
        const auto* valueText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.Get(), 0));
        MemoryEntry entry;
        entry.key   = key;
        entry.value = valueText ? valueText : "";
        return entry;
    }

    if (rc == SQLITE_DONE) {
        Logger::Warning("[SQLiteMemoryStorage] memory not found: " + key);
        return std::nullopt;
    }

    LogSqliteError(m_db.get(), "Load: failed to execute statement for key '" + key + "'");
    return std::nullopt;
}

bool SQLiteMemoryStorage::Remove(const std::string& key) {
    const char* sql = "DELETE FROM memories WHERE key = ?;";

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return false;
    }

    sqlite3_bind_text(stmt.Get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    return StepExpectDone(stmt.Get(), "Remove: failed to execute statement for key '" + key + "'");
}

bool SQLiteMemoryStorage::Clear() {
    const char* sql = "DELETE FROM memories;";

    return ExecuteNonQuery(sql);
}

std::vector<MemoryEntry> SQLiteMemoryStorage::LoadAll() {
    const char* sql = "SELECT key, value FROM memories;";

    std::vector<MemoryEntry> entries;

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return entries;
    }

    int rc = sqlite3_step(stmt.Get());
    while (rc == SQLITE_ROW) {
        const auto* keyText   = reinterpret_cast<const char*>(sqlite3_column_text(stmt.Get(), 0));
        const auto* valueText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.Get(), 1));

        MemoryEntry entry;
        entry.key   = keyText   ? keyText   : "";
        entry.value = valueText ? valueText : "";
        entries.push_back(std::move(entry));

        rc = sqlite3_step(stmt.Get());
    }

    if (rc != SQLITE_DONE) {
        LogSqliteError(m_db.get(), "LoadAll: failed while iterating rows");
    }

    return entries;
}

} // namespace Yuki::Memory