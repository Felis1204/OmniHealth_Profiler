#pragma once

#include <memory>
#include <string>

namespace health {

/// @brief 数据访问层 —— 封装 SQLite3 操作
///
/// 负责数据库初始化、表创建、数据增删改查及版本迁移。
/// 对外通过 JSON 字符串传递数据，与具体业务模型解耦。
class DataAccess {
public:
    DataAccess() = default;
    virtual ~DataAccess() = default;

    /// @brief 初始化数据库连接
    /// @param dbPath SQLite 数据库文件路径
    /// @return 成功返回 true
    virtual bool initialize(const std::string& dbPath) = 0;

    /// @brief 插入一条数据记录
    /// @param table 目标表名
    /// @param jsonValue JSON 格式的记录值
    /// @return 成功返回 true
    virtual bool insertRecord(const std::string& table, const std::string& jsonValue) = 0;

    /// @brief 通用查询接口
    /// @param sql SQL 查询语句
    /// @return JSON 格式的查询结果
    virtual std::string queryRecords(const std::string& sql) = 0;

    /// @brief 执行数据库版本迁移
    /// @param version 目标版本号
    /// @return 成功返回 true
    virtual bool executeMigration(int version) = 0;
};

/// @brief 工厂函数 —— 创建 DataAccess 实例
std::unique_ptr<DataAccess> createDataAccess();

} // namespace health