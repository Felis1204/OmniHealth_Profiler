#pragma once

namespace health {
class HealthManager;
}

/// @brief 向数据库插入一套中文场景的样例健康数据
///
/// 生成数据包括：
///   - 1 个用户档案（45 岁男性，北方城市，戒烟，无糖尿病）
///   - 过去 30 天的体征记录（心率、步数、睡眠、体重、腰围）
///   - 过去 30 天的血压记录（早晚各一次）
///   - 2 条临床检验记录（月初 + 月末对比）
///   - 3 条病历摘要记录
///
/// 数据覆盖范围足够测试以下功能：
///   - CRUD 列表展示
///   - 趋势折线图（多指标、多数据点）
///   - 风险评估（ASCVD / BMI / TyG / CDRS）
///   - AI 报告生成（需配置 API Key）
///   - 本地报告生成（降级兜底）
///
/// @param mgr 非空 HealthManager 指针
/// @return 成功插入的记录总数
int seedSampleData(health::HealthManager* mgr);
