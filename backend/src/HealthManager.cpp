#include "HealthManager.h"

#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace health {

// ============================================================
// 具体实现类 —— 后续会连接 SQLite 持久化 & 大模型 API
// ============================================================
class HealthManagerImpl : public HealthManager {
public:
    HealthManagerImpl() {
        std::cout << "[Backend] HealthManagerImpl 初始化完成" << std::endl;
    }

    ~HealthManagerImpl() override {
        std::cout << "[Backend] HealthManagerImpl 析构，资源已释放" << std::endl;
    }

    // ---- 添加记录 ----
    bool addRecord(const HealthRecord& record) override {
        try {
            records_.push_back(record);
            std::cout << "[Backend] 记录已添加: id=" << record.id
                      << ", type=" << static_cast<int>(record.type)
                      << ", value=" << record.value
                      << ", unit=" << record.unit << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[Backend] 添加记录失败: " << e.what() << std::endl;
            return false;
        }
    }

    // ---- 获取全部记录 ----
    std::vector<HealthRecord> getRecords() const override {
        return records_;
    }

    // ---- 按类型筛选 ----
    std::vector<HealthRecord> getRecordsByType(HealthMetricType type) const override {
        std::vector<HealthRecord> result;
        std::copy_if(records_.begin(), records_.end(), std::back_inserter(result),
                     [type](const HealthRecord& r) { return r.type == type; });
        return result;
    }

    // ---- 生成健康报告 (预留大模型 API) ----
    std::string generateHealthReport() const override {
        std::ostringstream oss;
        oss << "==================== 健康数字孪生报告 ====================\n";
        oss << "记录总数: " << records_.size() << "\n";
        oss << "============================================================\n";
        oss << "[提示] 完整 AI 分析报告将在接入大模型 API 后生成\n";
        oss << "============================================================\n";
        return oss.str();
    }

    // ---- 统计摘要 ----
    std::string getStatistics(HealthMetricType type) const override {
        auto filtered = getRecordsByType(type);
        if (filtered.empty()) {
            return "无该类型的健康记录";
        }

        double sum = 0.0;
        double minVal = filtered[0].value;
        double maxVal = filtered[0].value;
        for (const auto& r : filtered) {
            sum += r.value;
            minVal = std::min(minVal, r.value);
            maxVal = std::max(maxVal, r.value);
        }
        double avg = sum / static_cast<double>(filtered.size());

        std::ostringstream oss;
        oss << "类型: " << static_cast<int>(type) << "\n"
            << "记录数: " << filtered.size() << "\n"
            << "最小值: " << minVal << " " << (filtered.empty() ? "" : filtered[0].unit) << "\n"
            << "最大值: " << maxVal << " " << (filtered.empty() ? "" : filtered[0].unit) << "\n"
            << "平均值: " << avg << " " << (filtered.empty() ? "" : filtered[0].unit);
        return oss.str();
    }

private:
    std::vector<HealthRecord> records_;  // 内存存储，后续替换为 SQLite
};

} // namespace health


// ============================================================
// 工厂函数 —— 前端通过此函数获取 HealthManager 实例
// ============================================================
std::unique_ptr<health::HealthManager> CreateHealthManager() {
    return std::make_unique<health::HealthManagerImpl>();
}