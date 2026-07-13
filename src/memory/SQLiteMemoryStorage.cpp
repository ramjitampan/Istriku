#include "SQLiteMemoryStorage.h"
#include "utils/Logger.h"

#include <sqlite3.h>

#include <ctime>
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
        return;
    }
    if (!MigrateSchema()) {
        Logger::Warning("[SQLiteMemoryStorage] schema migration had errors (may be benign)");
    }
}

SQLiteMemoryStorage::SQLiteMemoryStorage(const Yuki::Config::MemoryConfig& config)
    : SQLiteMemoryStorage(config.database)
{
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

bool SQLiteMemoryStorage::MigrateSchema() {
    // Each ALTER TABLE silently succeeds if the column already exists
    // (error is ignored — raw execution does not propagate failures).
    ExecuteRaw("ALTER TABLE memories ADD COLUMN created_at INTEGER NOT NULL DEFAULT 0;");
    ExecuteRaw("ALTER TABLE memories ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0;");
    ExecuteRaw("ALTER TABLE memories ADD COLUMN priority INTEGER NOT NULL DEFAULT 0;");
    ExecuteRaw("ALTER TABLE memories ADD COLUMN confidence REAL NOT NULL DEFAULT 1.0;");
    ExecuteRaw("ALTER TABLE memories ADD COLUMN mention_count INTEGER NOT NULL DEFAULT 0;");
    ExecuteRaw("ALTER TABLE memories ADD COLUMN previous_value TEXT NOT NULL DEFAULT '';");
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

bool SQLiteMemoryStorage::ExecuteRaw(const char* sql) const {
    if (!IsOpen()) return false;

    char* errMsg = nullptr;
    sqlite3_exec(m_db.get(), sql, nullptr, nullptr, &errMsg);

    if (errMsg) {
        // Benign — column may already exist
        sqlite3_free(errMsg);
    }

    return true;
}

// ===========================================================================
// RowToEntry — read all columns from the current row into a MemoryEntry
// ===========================================================================

MemoryEntry SQLiteMemoryStorage::RowToEntry(sqlite3_stmt* stmt) const {
    MemoryEntry entry;

    auto readText = [&](int col) -> std::string {
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
        return text ? text : "";
    };

    auto readInt = [&](int col) -> std::int64_t {
        return static_cast<std::int64_t>(sqlite3_column_int64(stmt, col));
    };

    auto readDouble = [&](int col) -> double {
        return sqlite3_column_double(stmt, col);
    };

    // Column order: key, value, created_at, updated_at, priority,
    //               confidence, mention_count, previous_value
    entry.key   = readText(0);
    entry.value = readText(1);

    entry.metadata.created_at      = readInt(2);
    entry.metadata.updated_at      = readInt(3);
    entry.metadata.priority        = static_cast<int>(readInt(4));
    entry.metadata.confidence      = static_cast<float>(readDouble(5));
    entry.metadata.mention_count   = static_cast<int>(readInt(6));
    entry.metadata.previous_value  = readText(7);

    return entry;
}

// ===========================================================================
// Public API
// ===========================================================================

bool SQLiteMemoryStorage::Save(const MemoryEntry& entry) {
    const char* sql =
        "INSERT INTO memories"
        "(key, value, created_at, updated_at, priority, confidence, "
        " mention_count, previous_value) "
        "VALUES(?, ?, "
        "       COALESCE(NULLIF(?, 0), "
        "               (SELECT created_at FROM memories WHERE key = ?), "
        "               CAST(strftime('%s', 'now') AS INTEGER)), "
        "       CAST(strftime('%s', 'now') AS INTEGER), "
        "       ?, ?, "
        "       COALESCE((SELECT mention_count + 1 FROM memories WHERE key = ?), 1), "
        "       COALESCE((SELECT value     FROM memories WHERE key = ?), '')) "
        "ON CONFLICT(key) DO UPDATE SET "
        "  value          = excluded.value, "
        "  updated_at     = excluded.updated_at, "
        "  priority       = excluded.priority, "
        "  confidence     = excluded.confidence, "
        "  mention_count  = mention_count + 1, "
        "  previous_value = (SELECT value FROM memories WHERE key = excluded.key);";

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return false;
    }

    sqlite3_bind_text(stmt.Get(), 1, entry.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.Get(), 2, entry.value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.Get(), 3, entry.metadata.created_at);
    sqlite3_bind_text(stmt.Get(), 4, entry.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.Get(),  5, entry.metadata.priority);
    sqlite3_bind_double(stmt.Get(), 6, static_cast<double>(entry.metadata.confidence));
    sqlite3_bind_text(stmt.Get(), 7, entry.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.Get(), 8, entry.key.c_str(), -1, SQLITE_TRANSIENT);

    return StepExpectDone(stmt.Get(),
        "Save: failed for key '" + entry.key + "'");
}

std::optional<MemoryEntry> SQLiteMemoryStorage::Load(const std::string& key) {
    const char* sql =
        "SELECT key, value, created_at, updated_at, priority, "
        "       confidence, mention_count, previous_value "
        "FROM memories WHERE key = ?;";

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt.Get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt.Get());

    if (rc == SQLITE_ROW) {
        return RowToEntry(stmt.Get());
    }

    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }

    LogSqliteError(m_db.get(), "Load: failed for key '" + key + "'");
    return std::nullopt;
}

bool SQLiteMemoryStorage::Remove(const std::string& key) {
    const char* sql = "DELETE FROM memories WHERE key = ?;";

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return false;
    }

    sqlite3_bind_text(stmt.Get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    return StepExpectDone(stmt.Get(),
        "Remove: failed for key '" + key + "'");
}

bool SQLiteMemoryStorage::Clear() {
    const char* sql = "DELETE FROM memories;";
    return ExecuteNonQuery(sql);
}

std::vector<MemoryEntry> SQLiteMemoryStorage::LoadAll() {
    const char* sql =
        "SELECT key, value, created_at, updated_at, priority, "
        "       confidence, mention_count, previous_value "
        "FROM memories ORDER BY priority DESC, updated_at DESC;";

    std::vector<MemoryEntry> entries;

    StmtGuard stmt;
    if (!PrepareStatement(sql, stmt.OutParam())) {
        return entries;
    }

    int rc = sqlite3_step(stmt.Get());
    while (rc == SQLITE_ROW) {
        entries.push_back(RowToEntry(stmt.Get()));
        rc = sqlite3_step(stmt.Get());
    }

    if (rc != SQLITE_DONE) {
        LogSqliteError(m_db.get(), "LoadAll: failed while iterating rows");
    }

    return entries;
}

} // namespace Yuki::Memory
