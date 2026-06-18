#include "DataAccess.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sqlite3.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace health {

// ============================================================
// 白名单 —— 只允许操作已知表，防止 SQL 注入
// ============================================================
static const std::vector<std::string>& getAllowedTables() {
    static const std::vector<std::string> tables = {
        "user_profile",
        "vitals_records",
        "lab_test_records",
        "blood_pressure_records"
    };
    return tables;
}

// ============================================================
// 列名映射 —— 每张表的列序与 SYSTEM_DESIGN.md §4.1 的 CREATE TABLE 保持一致
// ============================================================
static const std::vector<std::string>& getTableColumns(const std::string& table) {
    static const std::vector<std::string> userProfileCols = {
        "id", "name", "birth_date", "gender", "smoking_status",
        "region", "urban_rural", "family_history_ascvd", "has_diabetes", "created_at"
    };
    static const std::vector<std::string> vitalsCols = {
        "id", "user_id", "timestamp", "heart_rate", "steps", "sleep_hours",
        "weight_kg", "height_cm", "waist_cm", "source", "note"
    };
    static const std::vector<std::string> labTestCols = {
        "id", "user_id", "timestamp", "fasting_glucose", "total_cholesterol",
        "ldl_c", "hdl_c", "triglycerides", "uric_acid", "note"
    };
    static const std::vector<std::string> bpCols = {
        "id", "user_id", "timestamp", "systolic", "diastolic", "note"
    };

    if (table == "user_profile")            return userProfileCols;
    if (table == "vitals_records")          return vitalsCols;
    if (table == "lab_test_records")        return labTestCols;
    if (table == "blood_pressure_records")  return bpCols;

    static const std::vector<std::string> empty;
    return empty;
}

// ============================================================
// PIMPL 具体实现类
// ============================================================
class DataAccessImpl : public DataAccess {
public:
    DataAccessImpl() {
        std::cerr << "[DataAccess] DataAccessImpl 构造完成" << std::endl;
    }

    ~DataAccessImpl() override {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
            std::cerr << "[DataAccess] DataAccessImpl 析构，数据库连接已关闭" << std::endl;
        }
    }

    // ---- 初始化 ----
    bool initialize(const std::string& dbPath) override;

    // ---- CRUD ----
    bool insertRecord(const std::string& table, const std::string& jsonValue) override;
    bool deleteRecord(const std::string& table, const std::string& id) override;
    bool updateRecord(const std::string& table, const std::string& id, const std::string& jsonValue) override;
    std::string queryRecords(const std::string& sql) override;

    // ---- 版本迁移 ----
    bool executeMigration(int version) override;

private:
    sqlite3* db_ = nullptr;

    /// @brief 创建所有业务表与索引（幂等）
    bool createSchema();
};

// ============================================================
// createSchema —— 完全按 SYSTEM_DESIGN.md §4.1 建表
// ============================================================
bool DataAccessImpl::createSchema() {
    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS user_profile (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            birth_date TEXT,
            gender TEXT,
            smoking_status TEXT,
            region TEXT,
            urban_rural TEXT,
            family_history_ascvd INTEGER DEFAULT 0,
            has_diabetes INTEGER DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS vitals_records (
            id TEXT PRIMARY KEY,
            user_id TEXT,
            timestamp TEXT NOT NULL,
            heart_rate REAL,
            steps INTEGER,
            sleep_hours REAL,
            weight_kg REAL,
            height_cm REAL,
            waist_cm REAL,
            source TEXT,
            note TEXT,
            FOREIGN KEY (user_id) REFERENCES user_profile(id)
        );

        CREATE TABLE IF NOT EXISTS lab_test_records (
            id TEXT PRIMARY KEY,
            user_id TEXT,
            timestamp TEXT NOT NULL,
            fasting_glucose REAL,
            total_cholesterol REAL,
            ldl_c REAL,
            hdl_c REAL,
            triglycerides REAL,
            uric_acid REAL,
            note TEXT,
            FOREIGN KEY (user_id) REFERENCES user_profile(id)
        );

        CREATE TABLE IF NOT EXISTS blood_pressure_records (
            id TEXT PRIMARY KEY,
            user_id TEXT,
            timestamp TEXT NOT NULL,
            systolic INTEGER,
            diastolic INTEGER,
            note TEXT,
            FOREIGN KEY (user_id) REFERENCES user_profile(id)
        );

        CREATE INDEX IF NOT EXISTS idx_vitals_timestamp
            ON vitals_records(timestamp);
        CREATE INDEX IF NOT EXISTS idx_lab_timestamp
            ON lab_test_records(timestamp);
        CREATE INDEX IF NOT EXISTS idx_bp_timestamp
            ON blood_pressure_records(timestamp);
    )SQL";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] 建表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ============================================================
// initialize
// ============================================================
bool DataAccessImpl::initialize(const std::string& dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] 无法打开数据库 " << dbPath << ": "
                  << (db_ ? sqlite3_errmsg(db_) : "unknown error") << std::endl;
        return false;
    }

    // 启用 WAL 模式（提升并发读性能）
    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] WAL 启用警告: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        // 非致命，继续
    }

    // 启用外键约束
    rc = sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] 外键启用警告: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        // 非致命，继续
    }

    if (!createSchema()) {
        return false;
    }

    std::cerr << "[DataAccess] 数据库初始化完成: " << dbPath << std::endl;
    return true;
}

// ============================================================
// insertRecord
// ============================================================
bool DataAccessImpl::insertRecord(const std::string& table,
                                   const std::string& jsonValue) {
    if (!db_) {
        std::cerr << "[DataAccess] insertRecord 失败: 数据库未初始化" << std::endl;
        return false;
    }

    // ---- 1. 表名白名单校验 ----
    const auto& allowed = getAllowedTables();
    if (std::find(allowed.begin(), allowed.end(), table) == allowed.end()) {
        std::cerr << "[DataAccess] insertRecord 失败: 非法表名 " << table << std::endl;
        return false;
    }

    // ---- 2. 解析 JSON ----
    json j;
    try {
        j = json::parse(jsonValue);
    } catch (const json::parse_error& e) {
        std::cerr << "[DataAccess] JSON 解析失败: " << e.what() << std::endl;
        return false;
    }

    if (!j.is_object()) {
        std::cerr << "[DataAccess] JSON 根元素必须是对象" << std::endl;
        return false;
    }

    // ---- 3. 获取列映射 ----
    const auto& columns = getTableColumns(table);
    if (columns.empty()) {
        std::cerr << "[DataAccess] 无法获取表 " << table << " 的列定义" << std::endl;
        return false;
    }

    // ---- 4. 构建 INSERT SQL（列名来自白名单映射，安全拼接）----
    std::string placeholders;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) placeholders += ", ";
        placeholders += "?";
    }

    std::string sql = "INSERT INTO " + table + " (";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) sql += ", ";
        sql += columns[i];
    }
    sql += ") VALUES (" + placeholders + ")";

    // ---- 5. 预编译语句 ----
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] SQL 预编译失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // ---- 6. 按列序绑定值 ----
    for (size_t i = 0; i < columns.size(); ++i) {
        int bindIdx = static_cast<int>(i) + 1; // SQLite 绑定从 1 开始
        const std::string& col = columns[i];

        if (j.contains(col)) {
            const auto& val = j[col];

            if (val.is_null()) {
                rc = sqlite3_bind_null(stmt, bindIdx);
            } else if (val.is_string()) {
                const std::string& s = val.get_ref<const std::string&>();
                rc = sqlite3_bind_text(stmt, bindIdx, s.c_str(), -1, SQLITE_TRANSIENT);
            } else if (val.is_number_integer()) {
                rc = sqlite3_bind_int64(stmt, bindIdx, val.get<int64_t>());
            } else if (val.is_number_float()) {
                rc = sqlite3_bind_double(stmt, bindIdx, val.get<double>());
            } else if (val.is_boolean()) {
                rc = sqlite3_bind_int(stmt, bindIdx, val.get<bool>() ? 1 : 0);
            } else {
                std::cerr << "[DataAccess] 不支持的 JSON 类型，列 " << col << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }
        } else {
            // JSON 中缺失该列 → 绑定 NULL
            rc = sqlite3_bind_null(stmt, bindIdx);
        }

        if (rc != SQLITE_OK) {
            std::cerr << "[DataAccess] 参数绑定失败，列 " << col
                      << ": " << sqlite3_errmsg(db_) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
    }

    // ---- 7. 执行 ----
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "[DataAccess] INSERT 执行失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return true;
}

// ============================================================
// deleteRecord
// ============================================================
bool DataAccessImpl::deleteRecord(const std::string& table,
                                   const std::string& id) {
    if (!db_) {
        std::cerr << "[DataAccess] deleteRecord 失败: 数据库未初始化" << std::endl;
        return false;
    }

    // 表名白名单校验
    const auto& allowed = getAllowedTables();
    if (std::find(allowed.begin(), allowed.end(), table) == allowed.end()) {
        std::cerr << "[DataAccess] deleteRecord 失败: 非法表名 " << table << std::endl;
        return false;
    }

    std::string sql = "DELETE FROM " + table + " WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] DELETE 预编译失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "[DataAccess] DELETE 执行失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return true; // 幂等：记录不存在也返回 true
}

// ============================================================
// updateRecord
// ============================================================
bool DataAccessImpl::updateRecord(const std::string& table,
                                   const std::string& id,
                                   const std::string& jsonValue) {
    if (!db_) {
        std::cerr << "[DataAccess] updateRecord 失败: 数据库未初始化" << std::endl;
        return false;
    }

    // 表名白名单校验
    const auto& allowed = getAllowedTables();
    if (std::find(allowed.begin(), allowed.end(), table) == allowed.end()) {
        std::cerr << "[DataAccess] updateRecord 失败: 非法表名 " << table << std::endl;
        return false;
    }

    // 解析 JSON
    json j;
    try {
        j = json::parse(jsonValue);
    } catch (const json::parse_error& e) {
        std::cerr << "[DataAccess] JSON 解析失败: " << e.what() << std::endl;
        return false;
    }

    if (!j.is_object()) {
        std::cerr << "[DataAccess] JSON 根元素必须是对象" << std::endl;
        return false;
    }

    // 获取列映射（排除 id，因为 id 作为 WHERE 条件）
    const auto& columns = getTableColumns(table);
    if (columns.empty()) {
        std::cerr << "[DataAccess] 无法获取表 " << table << " 的列定义" << std::endl;
        return false;
    }

    // 构建 UPDATE SQL: UPDATE table SET col1=?, col2=?, ... WHERE id = ?
    std::string sql = "UPDATE " + table + " SET ";
    bool first = true;
    for (const auto& col : columns) {
        if (col == "id") continue; // id 放 WHERE 子句
        if (j.contains(col)) {
            if (!first) sql += ", ";
            sql += col + " = ?";
            first = false;
        }
    }

    if (first) {
        std::cerr << "[DataAccess] updateRecord: JSON 中没有可更新的列" << std::endl;
        return false;
    }

    sql += " WHERE id = ?";

    // 预编译
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] UPDATE 预编译失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // 绑定 SET 子句的值（按 JSON 中出现顺序），然后绑定 WHERE id
    int bindIdx = 1;
    for (const auto& col : columns) {
        if (col == "id") continue;
        if (!j.contains(col)) continue;

        const auto& val = j[col];
        if (val.is_null()) {
            rc = sqlite3_bind_null(stmt, bindIdx);
        } else if (val.is_string()) {
            const std::string& s = val.get_ref<const std::string&>();
            rc = sqlite3_bind_text(stmt, bindIdx, s.c_str(), -1, SQLITE_TRANSIENT);
        } else if (val.is_number_integer()) {
            rc = sqlite3_bind_int64(stmt, bindIdx, val.get<int64_t>());
        } else if (val.is_number_float()) {
            rc = sqlite3_bind_double(stmt, bindIdx, val.get<double>());
        } else if (val.is_boolean()) {
            rc = sqlite3_bind_int(stmt, bindIdx, val.get<bool>() ? 1 : 0);
        } else {
            std::cerr << "[DataAccess] 不支持的 JSON 类型，列 " << col << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }

        if (rc != SQLITE_OK) {
            std::cerr << "[DataAccess] UPDATE 参数绑定失败，列 " << col
                      << ": " << sqlite3_errmsg(db_) << std::endl;
            sqlite3_finalize(stmt);
            return false;
        }
        ++bindIdx;
    }

    // 绑定 WHERE id
    sqlite3_bind_text(stmt, bindIdx, id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "[DataAccess] UPDATE 执行失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return changes > 0; // 有行被更新才返回 true
}

// ============================================================
// queryRecords
// ============================================================
std::string DataAccessImpl::queryRecords(const std::string& sql) {
    if (!db_) {
        std::cerr << "[DataAccess] queryRecords 失败: 数据库未初始化" << std::endl;
        return "[]";
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[DataAccess] 查询预编译失败: " << sqlite3_errmsg(db_) << std::endl;
        return "[]";
    }

    json resultArray = json::array();

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        json row = json::object();
        int colCount = sqlite3_column_count(stmt);

        for (int i = 0; i < colCount; ++i) {
            const char* colName = sqlite3_column_name(stmt, i);
            std::string key = colName ? std::string(colName) : "";
            int colType = sqlite3_column_type(stmt, i);

            switch (colType) {
                case SQLITE_INTEGER:
                    row[key] = sqlite3_column_int64(stmt, i);
                    break;
                case SQLITE_FLOAT:
                    row[key] = sqlite3_column_double(stmt, i);
                    break;
                case SQLITE_TEXT: {
                    const unsigned char* text = sqlite3_column_text(stmt, i);
                    row[key] = text ? std::string(reinterpret_cast<const char*>(text)) : "";
                    break;
                }
                case SQLITE_BLOB:
                    row[key] = "[BLOB]";
                    break;
                case SQLITE_NULL:
                default:
                    row[key] = nullptr;
                    break;
            }
        }

        resultArray.push_back(std::move(row));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "[DataAccess] 查询执行异常: " << sqlite3_errmsg(db_) << std::endl;
        // 返回已读取的部分结果，而非空数组
    }

    return resultArray.dump();
}

// ============================================================
// executeMigration
// ============================================================
bool DataAccessImpl::executeMigration(int version) {
    std::cerr << "[DataAccess] 请求执行数据库迁移至版本 " << version
              << " —— 功能尚未实现" << std::endl;
    // TODO(Felis1204): 实现基于 version 表的多版本增量迁移
    return true;
}

} // namespace health

// ============================================================
// 工厂函数 —— 前端通过此函数获取 DataAccess 实例
// ============================================================
std::unique_ptr<health::DataAccess> health::createDataAccess() {
    return std::make_unique<health::DataAccessImpl>();
}
