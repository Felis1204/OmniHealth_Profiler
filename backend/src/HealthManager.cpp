#include "HealthManager.h"
#include "DataAccess.h"
#include "ASCVDCalculator.h"
#include "LLMService.h"
#include "PlatformCompat.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace health {

// ============================================================
// 时间转换工具（TimePoint ↔ ISO 8601 字符串）
// ============================================================

static std::string timePointToIso(TimePoint tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = {};
    health::platform::gmtimeCompat(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static TimePoint isoToTimePoint(const std::string& iso) {
    std::tm tm = {};
    std::istringstream ss(iso);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        return TimePoint{};
    }
    auto t = health::platform::timegmCompat(&tm);
    return std::chrono::system_clock::from_time_t(t);
}

// ============================================================
// 序列化 —— struct → JSON object
// ============================================================

static json vitalsToJson(const VitalsRecord& r) {
    json j;
    j["id"]        = r.id;
    j["timestamp"] = timePointToIso(r.timestamp);
    if (r.heartRate)  j["heart_rate"]  = *r.heartRate;
    if (r.steps)      j["steps"]       = *r.steps;
    if (r.sleepHours) j["sleep_hours"] = *r.sleepHours;
    if (r.weightKg)   j["weight_kg"]   = *r.weightKg;
    if (r.heightCm)   j["height_cm"]   = *r.heightCm;
    if (r.waistCm)    j["waist_cm"]    = *r.waistCm;
    if (r.source)     j["source"]      = *r.source;
    if (r.note)       j["note"]        = *r.note;
    return j;
}

static json labTestToJson(const LabTestRecord& r) {
    json j;
    j["id"]        = r.id;
    j["timestamp"] = timePointToIso(r.timestamp);
    if (r.fastingGlucose)   j["fasting_glucose"]   = *r.fastingGlucose;
    if (r.totalCholesterol) j["total_cholesterol"] = *r.totalCholesterol;
    if (r.ldlC)             j["ldl_c"]             = *r.ldlC;
    if (r.hdlC)             j["hdl_c"]             = *r.hdlC;
    if (r.triglycerides)    j["triglycerides"]     = *r.triglycerides;
    if (r.uricAcid)         j["uric_acid"]         = *r.uricAcid;
    if (r.source)           j["source"]            = *r.source;
    if (r.note)             j["note"]              = *r.note;
    return j;
}

static json bpToJson(const BloodPressureRecord& r) {
    json j;
    j["id"]        = r.id;
    j["timestamp"] = timePointToIso(r.timestamp);
    if (r.systolic)  j["systolic"]  = *r.systolic;
    if (r.diastolic) j["diastolic"] = *r.diastolic;
    if (r.source)    j["source"]    = *r.source;
    if (r.note)      j["note"]      = *r.note;
    return j;
}

// ============================================================
// 反序列化 —— JSON object → struct
// ============================================================

static VitalsRecord jsonToVitals(const json& j) {
    VitalsRecord r;
    r.id         = j.value("id", "");
    r.recordType = HealthRecordType::VITALS;
    r.timestamp  = isoToTimePoint(j.value("timestamp", ""));

    auto setOptDouble = [&](const char* key, std::optional<double>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<double>();
    };
    auto setOptInt = [&](const char* key, std::optional<int>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<int>();
    };
    auto setOptStr = [&](const char* key, std::optional<std::string>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<std::string>();
    };

    setOptDouble("heart_rate",  r.heartRate);
    setOptInt   ("steps",       r.steps);
    setOptDouble("sleep_hours", r.sleepHours);
    setOptDouble("weight_kg",   r.weightKg);
    setOptDouble("height_cm",   r.heightCm);
    setOptDouble("waist_cm",    r.waistCm);
    setOptStr   ("source",      r.source);
    setOptStr   ("note",        r.note);
    return r;
}

static LabTestRecord jsonToLabTest(const json& j) {
    LabTestRecord r;
    r.id         = j.value("id", "");
    r.recordType = HealthRecordType::LAB_TEST;
    r.timestamp  = isoToTimePoint(j.value("timestamp", ""));

    auto setOptDouble = [&](const char* key, std::optional<double>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<double>();
    };
    auto setOptStr = [&](const char* key, std::optional<std::string>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<std::string>();
    };

    setOptDouble("fasting_glucose",   r.fastingGlucose);
    setOptDouble("total_cholesterol", r.totalCholesterol);
    setOptDouble("ldl_c",             r.ldlC);
    setOptDouble("hdl_c",             r.hdlC);
    setOptDouble("triglycerides",     r.triglycerides);
    setOptDouble("uric_acid",         r.uricAcid);
    setOptStr   ("source",            r.source);
    setOptStr   ("note",              r.note);
    return r;
}

static BloodPressureRecord jsonToBp(const json& j) {
    BloodPressureRecord r;
    r.id         = j.value("id", "");
    r.recordType = HealthRecordType::BP;
    r.timestamp  = isoToTimePoint(j.value("timestamp", ""));

    auto setOptInt = [&](const char* key, std::optional<int>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<int>();
    };
    auto setOptStr = [&](const char* key, std::optional<std::string>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<std::string>();
    };

    setOptInt("systolic",  r.systolic);
    setOptInt("diastolic", r.diastolic);
    setOptStr("source",    r.source);
    setOptStr("note",      r.note);
    return r;
}

// ============================================================
// UserProfile 序列化
// ============================================================

static json userProfileToJson(const UserProfile& p) {
    json j;
    j["id"]   = p.id;
    j["name"] = p.name;
    if (p.birthDate)          j["birth_date"]          = *p.birthDate;
    if (p.gender)             j["gender"]              = *p.gender;
    if (p.smokingStatus)      j["smoking_status"]      = *p.smokingStatus;
    if (p.region)             j["region"]              = *p.region;
    if (p.urbanRural)         j["urban_rural"]         = *p.urbanRural;
    if (p.familyHistoryASCVD) j["family_history_ascvd"] = *p.familyHistoryASCVD ? 1 : 0;
    if (p.hasDiabetes)        j["has_diabetes"]        = *p.hasDiabetes ? 1 : 0;
    return j;
}

static UserProfile jsonToUserProfile(const json& j) {
    UserProfile p;
    p.id   = j.value("id", "");
    p.name = j.value("name", "");

    auto setOptStr = [&](const char* key, std::optional<std::string>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = j[key].get<std::string>();
    };
    auto setOptBool = [&](const char* key, std::optional<bool>& dest) {
        if (j.contains(key) && !j[key].is_null())
            dest = (j[key].get<int>() != 0);
    };

    setOptStr ("birth_date",           p.birthDate);
    setOptStr ("gender",               p.gender);
    setOptStr ("smoking_status",       p.smokingStatus);
    setOptStr ("region",               p.region);
    setOptStr ("urban_rural",          p.urbanRural);
    setOptBool("family_history_ascvd", p.familyHistoryASCVD);
    setOptBool("has_diabetes",         p.hasDiabetes);
    return p;
}

// ============================================================
// 年龄计算（身份证年龄：满周岁）
// ============================================================

static int calculateAge(const std::optional<std::string>& birthDate) {
    if (!birthDate || birthDate->empty()) return 0;

    std::tm tm = {};
    std::istringstream ss(*birthDate);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        // 尝试仅年份格式
        std::istringstream ss2(*birthDate);
        ss2 >> std::get_time(&tm, "%Y");
        if (ss2.fail()) return 0;
    }

    auto birthTime = std::chrono::system_clock::from_time_t(health::platform::timegmCompat(&tm));
    auto now = std::chrono::system_clock::now();
    auto hours = std::chrono::duration_cast<std::chrono::hours>(now - birthTime).count();
    return static_cast<int>(hours / (365.25 * 24.0));
}

// ============================================================
// 统计计算工具
// ============================================================

/// @brief 计算中位数（会修改输入 vector 的顺序）
static double median(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    size_t n = v.size();
    std::nth_element(v.begin(), v.begin() + n / 2, v.end());
    double mid = v[n / 2];
    if (n % 2 == 0) {
        std::nth_element(v.begin(), v.begin() + n / 2 - 1, v.end());
        return (mid + v[n / 2 - 1]) / 2.0;
    }
    return mid;
}

/// @brief 简单线性回归斜率（y = a + b*x，返回 b）
static double linearSlope(const std::vector<double>& y) {
    if (y.size() < 2) return 0.0;
    size_t n = y.size();
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        sumX  += x;
        sumY  += y[i];
        sumXY += x * y[i];
        sumX2 += x * x;
    }
    double denom = n * sumX2 - sumX * sumX;
    if (std::abs(denom) < 1e-9) return 0.0;
    return (n * sumXY - sumX * sumY) / denom;
}

// ============================================================
// 趋势分析 —— 指标定义映射
// ============================================================

/// @brief 单个量化指标的定义（列名 → 中文名 + 单位）
struct MetricDef {
    std::string column;   // 数据库列名
    std::string label;    // 中文显示名
    std::string unit;     // 单位
};

/// @brief 获取指定记录类型的所有量化指标定义
static std::vector<MetricDef> getMetricDefinitions(HealthRecordType type) {
    switch (type) {
        case HealthRecordType::VITALS:
            return {
                {"heart_rate",  "心率",   "bpm"},
                {"steps",       "步数",   "步"},
                {"sleep_hours", "睡眠",   "小时"},
                {"weight_kg",   "体重",   "kg"},
                {"height_cm",   "身高",   "cm"},
                {"waist_cm",    "腰围",   "cm"}
            };
        case HealthRecordType::LAB_TEST:
            return {
                {"fasting_glucose",   "空腹血糖",  "mmol/L"},
                {"total_cholesterol", "总胆固醇",  "mmol/L"},
                {"ldl_c",             "LDL-C",     "mmol/L"},
                {"hdl_c",             "HDL-C",     "mmol/L"},
                {"triglycerides",     "甘油三酯",  "mmol/L"},
                {"uric_acid",         "血尿酸",    "µmol/L"}
            };
        case HealthRecordType::BP:
            return {
                {"systolic",  "收缩压", "mmHg"},
                {"diastolic", "舒张压", "mmHg"}
            };
        case HealthRecordType::HISTORY:
            return {};  // 病历摘要无量化指标
    }
    return {};
}

/// @brief 获取记录类型对应的表名
static std::string getTableForType(HealthRecordType type) {
    switch (type) {
        case HealthRecordType::VITALS:   return "vitals_records";
        case HealthRecordType::LAB_TEST: return "lab_test_records";
        case HealthRecordType::BP:       return "blood_pressure_records";
        case HealthRecordType::HISTORY:  return "";
    }
    return "";
}

/// @brief 获取趋势报告标题
static std::string getReportTitle(HealthRecordType type) {
    switch (type) {
        case HealthRecordType::VITALS:   return "体征指标趋势分析";
        case HealthRecordType::LAB_TEST: return "临床检验指标趋势分析";
        case HealthRecordType::BP:       return "血压趋势分析";
        case HealthRecordType::HISTORY:  return "病历摘要";
    }
    return "";
}

/// @brief 格式化时间范围标签
static std::string formatPeriodLabel(
    std::optional<TimePoint> from,
    std::optional<TimePoint> to)
{
    std::string label;
    if (from) label += timePointToIso(*from).substr(0, 10);
    else      label += "最早记录";
    label += " 至 ";
    if (to) label += timePointToIso(*to).substr(0, 10);
    else    label += "最新记录";
    return label;
}

// ============================================================
// PIMPL 具体实现类
// ============================================================
class HealthManagerImpl : public HealthManager {
public:
    HealthManagerImpl()
        : dataAccess_(createDataAccess()),
          llmService_(createLLMService())
    {
        std::cerr << "[Backend] HealthManagerImpl 初始化开始" << std::endl;

        if (!dataAccess_->initialize("omnihealth.db")) {
            std::cerr << "[Backend] 警告: 数据库初始化失败，部分功能不可用" << std::endl;
        }

        // 尝试配置 LLMService（从环境变量读取 API Key）
        llmService_->configure(
            "https://api.deepseek.com/chat/completions",
            "",   // 空字符串 → 从环境变量 OPENAI_API_KEY 读取
            "deepseek-chat"
        );
        if (llmService_->isConfigured()) {
            std::cerr << "[Backend] AI 顾问已就绪" << std::endl;
        } else {
            std::cerr << "[Backend] AI 顾问未配置，将使用本地报告" << std::endl;
        }

        std::cerr << "[Backend] HealthManagerImpl 初始化完成" << std::endl;
    }

    ~HealthManagerImpl() override {
        std::cerr << "[Backend] HealthManagerImpl 析构，资源已释放" << std::endl;
    }

    // ---- CRUD ----
    bool addVitalsRecord(const VitalsRecord& record) override;
    bool addLabTestRecord(const LabTestRecord& record) override;
    bool addBloodPressureRecord(const BloodPressureRecord& record) override;

    bool updateVitalsRecord(const VitalsRecord& record) override;
    bool updateLabTestRecord(const LabTestRecord& record) override;
    bool updateBloodPressureRecord(const BloodPressureRecord& record) override;

    bool deleteVitalsRecord(const std::string& id) override;
    bool deleteLabTestRecord(const std::string& id) override;
    bool deleteBloodPressureRecord(const std::string& id) override;

    std::vector<VitalsRecord> getVitalsRecords(
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const override;

    std::vector<BloodPressureRecord> getBloodPressureRecords(
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const override;

    std::vector<LabTestRecord> getLabTestRecords(
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const override;

    bool saveUserProfile(const UserProfile& profile) override;
    std::optional<UserProfile> getUserProfile() const override;
    bool deleteUserProfile(const std::string& id) override;

    // ---- 风险计算 ----
    double calculateASCVDScore() const override;
    double calculateBMI() const override;
    std::string getBMICategory() const override;

    // ---- 趋势分析 ----
    TrendResult analyzeTrend(HealthRecordType type,
        TimePoint from, TimePoint to) const override;
    TrendReport analyzeTrendReport(HealthRecordType type,
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const override;

    // ---- 统计摘要 ----
    std::string getStatistics(HealthRecordType type) const override;

    // ---- 健康报告 ----
    std::string generateHealthReport() const override;
    std::string generateHealthReport(ReportPeriod period) const override;
    std::string generateAIReport(ReportPeriod period) override;

    // ---- LLM 咨询 ----
    std::string askHealthAdvisor(const std::string& userQuery) const override;

private:
    std::unique_ptr<DataAccess> dataAccess_;
    std::unique_ptr<LLMService> llmService_;

    /// @brief 构建带时间范围过滤的 SQL
    std::string buildTimeRangeQuery(
        const std::string& table,
        const std::string& selectCols,
        std::optional<TimePoint> from,
        std::optional<TimePoint> to) const;

    /// @brief 获取最近 N 次收缩压均值（临床建议 2-3 次）
    double getAverageSystolicBP(int count) const;
};

// ============================================================
// buildTimeRangeQuery
// ============================================================
std::string HealthManagerImpl::buildTimeRangeQuery(
    const std::string& table,
    const std::string& selectCols,
    std::optional<TimePoint> from,
    std::optional<TimePoint> to) const
{
    std::ostringstream sql;
    sql << "SELECT " << selectCols << " FROM " << table;
    std::vector<std::string> conditions;
    if (from) conditions.push_back("timestamp >= '" + timePointToIso(*from) + "'");
    if (to)   conditions.push_back("timestamp <= '" + timePointToIso(*to) + "'");
    if (!conditions.empty()) {
        sql << " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) sql << " AND ";
            sql << conditions[i];
        }
    }
    sql << " ORDER BY timestamp ASC";
    return sql.str();
}

// ============================================================
// CRUD 实现
// ============================================================

bool HealthManagerImpl::addVitalsRecord(const VitalsRecord& record) {
    std::cerr << "[Backend] addVitalsRecord: id=" << record.id << std::endl;
    json j = vitalsToJson(record);
    return dataAccess_->insertRecord("vitals_records", j.dump());
}

bool HealthManagerImpl::addLabTestRecord(const LabTestRecord& record) {
    std::cerr << "[Backend] addLabTestRecord: id=" << record.id << std::endl;
    json j = labTestToJson(record);
    return dataAccess_->insertRecord("lab_test_records", j.dump());
}

bool HealthManagerImpl::addBloodPressureRecord(const BloodPressureRecord& record) {
    std::cerr << "[Backend] addBloodPressureRecord: id=" << record.id << std::endl;
    json j = bpToJson(record);
    return dataAccess_->insertRecord("blood_pressure_records", j.dump());
}

// ---- Update ----

bool HealthManagerImpl::updateVitalsRecord(const VitalsRecord& record) {
    std::cerr << "[Backend] updateVitalsRecord: id=" << record.id << std::endl;
    json j = vitalsToJson(record);
    return dataAccess_->updateRecord("vitals_records", record.id, j.dump());
}

bool HealthManagerImpl::updateLabTestRecord(const LabTestRecord& record) {
    std::cerr << "[Backend] updateLabTestRecord: id=" << record.id << std::endl;
    json j = labTestToJson(record);
    return dataAccess_->updateRecord("lab_test_records", record.id, j.dump());
}

bool HealthManagerImpl::updateBloodPressureRecord(const BloodPressureRecord& record) {
    std::cerr << "[Backend] updateBloodPressureRecord: id=" << record.id << std::endl;
    json j = bpToJson(record);
    return dataAccess_->updateRecord("blood_pressure_records", record.id, j.dump());
}

// ---- Delete ----

bool HealthManagerImpl::deleteVitalsRecord(const std::string& id) {
    std::cerr << "[Backend] deleteVitalsRecord: id=" << id << std::endl;
    return dataAccess_->deleteRecord("vitals_records", id);
}

bool HealthManagerImpl::deleteLabTestRecord(const std::string& id) {
    std::cerr << "[Backend] deleteLabTestRecord: id=" << id << std::endl;
    return dataAccess_->deleteRecord("lab_test_records", id);
}

bool HealthManagerImpl::deleteBloodPressureRecord(const std::string& id) {
    std::cerr << "[Backend] deleteBloodPressureRecord: id=" << id << std::endl;
    return dataAccess_->deleteRecord("blood_pressure_records", id);
}

std::vector<VitalsRecord> HealthManagerImpl::getVitalsRecords(
    std::optional<TimePoint> from,
    std::optional<TimePoint> to) const
{
    std::string sql = buildTimeRangeQuery("vitals_records", "*", from, to);
    std::string resultJson = dataAccess_->queryRecords(sql);

    std::vector<VitalsRecord> records;
    try {
        json arr = json::parse(resultJson);
        for (const auto& j : arr) {
            records.push_back(jsonToVitals(j));
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[Backend] getVitalsRecords JSON 解析失败: " << e.what() << std::endl;
    }
    return records;
}

std::vector<BloodPressureRecord> HealthManagerImpl::getBloodPressureRecords(
    std::optional<TimePoint> from,
    std::optional<TimePoint> to) const
{
    std::string sql = buildTimeRangeQuery("blood_pressure_records", "*", from, to);
    std::string resultJson = dataAccess_->queryRecords(sql);

    std::vector<BloodPressureRecord> records;
    try {
        json arr = json::parse(resultJson);
        for (const auto& j : arr) {
            records.push_back(jsonToBp(j));
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[Backend] getBloodPressureRecords JSON 解析失败: "
                  << e.what() << std::endl;
    }
    return records;
}

std::vector<LabTestRecord> HealthManagerImpl::getLabTestRecords(
    std::optional<TimePoint> from,
    std::optional<TimePoint> to) const
{
    std::string sql = buildTimeRangeQuery("lab_test_records", "*", from, to);
    std::string resultJson = dataAccess_->queryRecords(sql);

    std::vector<LabTestRecord> records;
    try {
        json arr = json::parse(resultJson);
        for (const auto& j : arr) {
            records.push_back(jsonToLabTest(j));
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[Backend] getLabTestRecords JSON 解析失败: " << e.what() << std::endl;
    }
    return records;
}

bool HealthManagerImpl::saveUserProfile(const UserProfile& profile) {
    std::cerr << "[Backend] saveUserProfile: id=" << profile.id << std::endl;
    json j = userProfileToJson(profile);
    // Upsert: 先尝试更新，不存在则插入
    if (dataAccess_->updateRecord("user_profile", profile.id, j.dump())) {
        return true;
    }
    return dataAccess_->insertRecord("user_profile", j.dump());
}

bool HealthManagerImpl::deleteUserProfile(const std::string& id) {
    std::cerr << "[Backend] deleteUserProfile: id=" << id << std::endl;
    return dataAccess_->deleteRecord("user_profile", id);
}

std::optional<UserProfile> HealthManagerImpl::getUserProfile() const {
    std::string sql = "SELECT * FROM user_profile LIMIT 1";
    std::string resultJson = dataAccess_->queryRecords(sql);
    try {
        json arr = json::parse(resultJson);
        if (!arr.empty()) {
            return jsonToUserProfile(arr[0]);
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[Backend] getUserProfile JSON 解析失败: " << e.what() << std::endl;
    }
    return std::nullopt;
}

double HealthManagerImpl::getAverageSystolicBP(int count) const {
    auto bps = getBloodPressureRecords(std::nullopt, std::nullopt);
    if (bps.empty()) return 0.0;

    double sum = 0.0;
    int n = 0;
    // 倒序遍历取最近 N 次有收缩压的记录
    for (auto it = bps.rbegin(); it != bps.rend() && n < count; ++it) {
        if (it->systolic) {
            sum += static_cast<double>(*it->systolic);
            ++n;
        }
    }
    return (n > 0) ? sum / static_cast<double>(n) : 0.0;
}

// ============================================================
// 风险计算
// ============================================================

double HealthManagerImpl::calculateBMI() const {
    // 查询最新一条同时有体重和身高的记录
    std::string sql =
        "SELECT weight_kg, height_cm FROM vitals_records "
        "WHERE weight_kg IS NOT NULL AND height_cm IS NOT NULL "
        "ORDER BY timestamp DESC LIMIT 1";

    std::string resultJson = dataAccess_->queryRecords(sql);
    try {
        json arr = json::parse(resultJson);
        if (!arr.empty()) {
            double weight = arr[0].value("weight_kg", 0.0);
            double heightCm = arr[0].value("height_cm", 0.0);
            if (heightCm > 0) {
                double heightM = heightCm / 100.0;
                return weight / (heightM * heightM);
            }
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[Backend] calculateBMI JSON 解析失败: " << e.what() << std::endl;
    }

    return 0.0;
}

std::string HealthManagerImpl::getBMICategory() const {
    double bmi = calculateBMI();
    if (bmi <= 0.0)  return "暂无数据";
    if (bmi < 18.5)  return "偏瘦";
    if (bmi < 24.0)  return "正常";
    if (bmi < 28.0)  return "超重";
    return "肥胖";
}

double HealthManagerImpl::calculateASCVDScore() const {
    std::cerr << "[Backend] 计算 China-PAR ASCVD 风险评分..." << std::endl;

    // ---- 1. 获取用户档案 ----
    auto profileOpt = getUserProfile();
    if (!profileOpt) {
        std::cerr << "[Backend] ASCVD: 缺少用户档案，返回 0" << std::endl;
        return 0.0;
    }
    const auto& p = *profileOpt;

    // ---- 2. 计算身份证年龄 ----
    int age = calculateAge(p.birthDate);
    if (age < 35 || age > 74) {
        std::cerr << "[Backend] ASCVD: 年龄 " << age
                  << " 超出 China-PAR 适用范围 (35-74)" << std::endl;
        return 0.0;
    }

    // ---- 3. 收缩压（近 3 次均值）----
    double avgSbp = getAverageSystolicBP(3);
    if (avgSbp <= 0.0) {
        std::cerr << "[Backend] ASCVD: 缺少血压数据" << std::endl;
        return 0.0;
    }

    // ---- 4. 最新血脂 ----
    auto labs = getLabTestRecords(std::nullopt, std::nullopt);
    if (labs.empty()) {
        std::cerr << "[Backend] ASCVD: 缺少临床检验数据" << std::endl;
        return 0.0;
    }
    const auto& lab = labs.back();
    if (!lab.totalCholesterol || !lab.hdlC || !lab.fastingGlucose) {
        std::cerr << "[Backend] ASCVD: 血脂或血糖数据不完整" << std::endl;
        return 0.0;
    }

    // ---- 5. 最新腰围 ----
    double waist = 0.0;
    auto vitals = getVitalsRecords(std::nullopt, std::nullopt);
    for (auto it = vitals.rbegin(); it != vitals.rend(); ++it) {
        if (it->waistCm) {
            waist = *it->waistCm;
            break;
        }
    }

    // ---- 6. 组装 ASCVDParams ----
    ASCVDParams params;
    params.age              = age;
    params.isMale           = (p.gender && *p.gender == "MALE");
    params.systolicBP       = avgSbp;
    params.fastingGlucose   = *lab.fastingGlucose;
    params.totalCholesterol = *lab.totalCholesterol;
    params.hdlC             = *lab.hdlC;
    params.waistCm          = waist;
    params.isCurrentSmoker  = (p.smokingStatus && *p.smokingStatus == "CURRENT");
    params.hasDiabetes      = (p.hasDiabetes && *p.hasDiabetes);
    params.isNorthern       = (!p.region || *p.region != "SOUTH");  // 默认北方
    params.isUrban          = (!p.urbanRural || *p.urbanRural != "RURAL"); // 默认城市
    params.hasFamilyHistory = (p.familyHistoryASCVD && *p.familyHistoryASCVD);

    // ---- 7. 调用 China-PAR 计算器 ----
    return ASCVDCalculator::calculateChinaPAR(params);
}

// ============================================================
// 趋势分析
// ============================================================

TrendResult HealthManagerImpl::analyzeTrend(
    HealthRecordType type,
    TimePoint from, TimePoint to) const
{
    TrendResult result{};
    result.average = 0.0;
    result.min    = 0.0;
    result.max    = 0.0;
    result.median = 0.0;
    result.slope  = 0.0;

    // 根据记录类型选择表名和默认指标列
    std::string table;
    std::string valueCol;

    switch (type) {
        case HealthRecordType::VITALS:
            table    = "vitals_records";
            valueCol = "heart_rate";
            break;
        case HealthRecordType::LAB_TEST:
            table    = "lab_test_records";
            valueCol = "fasting_glucose";
            break;
        case HealthRecordType::BP:
            table    = "blood_pressure_records";
            valueCol = "systolic";
            break;
        case HealthRecordType::HISTORY:
            // 病历没有数值指标
            return result;
    }

    // 查询时间范围内的数据
    std::ostringstream sql;
    sql << "SELECT " << valueCol << ", timestamp FROM " << table
        << " WHERE " << valueCol << " IS NOT NULL"
        << " AND timestamp >= '" << timePointToIso(from) << "'"
        << " AND timestamp <= '" << timePointToIso(to) << "'"
        << " ORDER BY timestamp ASC";

    std::string resultJson = dataAccess_->queryRecords(sql.str());
    std::vector<double> values;

    try {
        json arr = json::parse(resultJson);
        for (const auto& row : arr) {
            if (row.contains(valueCol) && !row[valueCol].is_null()) {
                values.push_back(row[valueCol].get<double>());
            }
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[Backend] analyzeTrend JSON 解析失败: " << e.what() << std::endl;
        return result;
    }

    if (values.empty()) {
        std::cerr << "[Backend] analyzeTrend: 指定时间范围内无数据" << std::endl;
        return result;
    }

    // 统计计算
    result.average = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    result.min     = *std::min_element(values.begin(), values.end());
    result.max     = *std::max_element(values.begin(), values.end());
    result.median  = median(values);
    result.slope   = linearSlope(values);

    return result;
}

// ============================================================
// 趋势分析报告（新版：所有指标 + 数据点 + 统计摘要）
// ============================================================

TrendReport HealthManagerImpl::analyzeTrendReport(
    HealthRecordType type,
    std::optional<TimePoint> from,
    std::optional<TimePoint> to) const
{
    TrendReport report;
    report.recordType = type;
    report.title      = getReportTitle(type);
    report.periodLabel = formatPeriodLabel(from, to);

    // 获取指标定义
    const auto metricDefs = getMetricDefinitions(type);
    if (metricDefs.empty()) return report;

    // 获取表名
    std::string table = getTableForType(type);
    if (table.empty()) return report;

    // 对每个量化指标查询时序数据
    for (const auto& def : metricDefs) {
        // 构建查询: SELECT timestamp, {col} FROM {table}
        //           WHERE {col} IS NOT NULL [AND timestamp >= from] [AND timestamp <= to]
        //           ORDER BY timestamp ASC
        std::ostringstream sql;
        sql << "SELECT timestamp, " << def.column
            << " FROM " << table
            << " WHERE " << def.column << " IS NOT NULL";

        if (from) {
            sql << " AND timestamp >= '" << timePointToIso(*from) << "'";
        }
        if (to) {
            sql << " AND timestamp <= '" << timePointToIso(*to) << "'";
        }
        sql << " ORDER BY timestamp ASC";

        std::string resultJson = dataAccess_->queryRecords(sql.str());

        // 解析结果
        std::vector<std::pair<std::string, double>> rows; // {timestamp, value}
        try {
            json arr = json::parse(resultJson);
            for (const auto& row : arr) {
                if (row.contains("timestamp") && row.contains(def.column) &&
                    !row["timestamp"].is_null() && !row[def.column].is_null()) {
                    rows.emplace_back(row["timestamp"].get<std::string>(),
                                      row[def.column].get<double>());
                }
            }
        } catch (const json::parse_error& e) {
            std::cerr << "[Backend] analyzeTrendReport JSON 解析失败 ("
                      << def.column << "): " << e.what() << std::endl;
            continue;
        }

        // 跳过无数据的指标
        if (rows.empty()) continue;

        // 构建 MetricTrend
        MetricTrend mt;
        mt.metricName = def.label;
        mt.metricKey  = def.column;
        mt.unit       = def.unit;
        mt.count      = static_cast<int>(rows.size());

        std::vector<double> values;
        for (const auto& [ts, val] : rows) {
            mt.dataPoints.push_back({ts, val});
            values.push_back(val);
        }

        // 统计摘要
        mt.average = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        mt.min     = *std::min_element(values.begin(), values.end());
        mt.max     = *std::max_element(values.begin(), values.end());
        mt.median  = median(values);
        mt.slope   = linearSlope(values);

        report.metrics.push_back(std::move(mt));
    }

    std::cerr << "[Backend] analyzeTrendReport: " << report.title
              << " — " << report.metrics.size() << " 个指标" << std::endl;

    return report;
}

// ============================================================
// 统计摘要
// ============================================================

std::string HealthManagerImpl::getStatistics(HealthRecordType type) const {
    // 根据记录类型选择表名和数值列
    std::string table;
    std::vector<std::pair<std::string, std::string>> columns; // {列名, 显示名}

    switch (type) {
        case HealthRecordType::VITALS:
            table = "vitals_records";
            columns = {
                {"heart_rate",  "心率"},
                {"steps",       "步数"},
                {"sleep_hours", "睡眠时长"},
                {"weight_kg",   "体重"},
                {"height_cm",   "身高"},
                {"waist_cm",    "腰围"}
            };
            break;
        case HealthRecordType::LAB_TEST:
            table = "lab_test_records";
            columns = {
                {"fasting_glucose",   "空腹血糖"},
                {"total_cholesterol", "总胆固醇"},
                {"ldl_c",             "LDL-C"},
                {"hdl_c",             "HDL-C"},
                {"triglycerides",     "甘油三酯"},
                {"uric_acid",         "尿酸"}
            };
            break;
        case HealthRecordType::BP:
            table = "blood_pressure_records";
            columns = {
                {"systolic",  "收缩压"},
                {"diastolic", "舒张压"}
            };
            break;
        case HealthRecordType::HISTORY:
            return "病历摘要暂无统计指标。";
    }

    std::ostringstream oss;
    oss << "========== 统计摘要 ==========\n";

    for (const auto& [col, label] : columns) {
        std::ostringstream sql;
        sql << "SELECT " << col << " FROM " << table
            << " WHERE " << col << " IS NOT NULL";
        std::string resultJson = dataAccess_->queryRecords(sql.str());

        std::vector<double> values;
        try {
            json arr = json::parse(resultJson);
            for (const auto& row : arr) {
                if (row.contains(col) && !row[col].is_null())
                    values.push_back(row[col].get<double>());
            }
        } catch (const json::parse_error&) {
            continue;
        }

        if (values.empty()) continue;

        double avg = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        double min = *std::min_element(values.begin(), values.end());
        double max = *std::max_element(values.begin(), values.end());

        oss << "[" << label << "]\n";
        oss << "  记录数: " << values.size() << "\n";
        oss << "  均值: " << std::fixed << std::setprecision(1) << avg << "\n";
        oss << "  最小值: " << min << "\n";
        oss << "  最大值: " << max << "\n";
    }

    if (oss.str() == "========== 统计摘要 ==========\n") {
        oss << "暂无数据。";
    }

    return oss.str();
}

// ============================================================
// LLM 咨询（增强桩 —— 无 LLMService 时使用真实数据生成摘要）
// ============================================================

std::string HealthManagerImpl::generateHealthReport() const {
    std::ostringstream oss;
    oss << "==================== 健康数字孪生报告 ====================\n\n";

    // 数据概览
    auto vitals = getVitalsRecords(std::nullopt, std::nullopt);
    auto labs   = getLabTestRecords(std::nullopt, std::nullopt);
    auto bps    = getBloodPressureRecords(std::nullopt, std::nullopt);

    oss << "【数据概览】\n";
    oss << "  - 体征记录: " << vitals.size() << " 条\n";
    oss << "  - 临床检验: " << labs.size()   << " 条\n";
    oss << "  - 血压记录: " << bps.size()    << " 条\n";

    // BMI
    double bmi = calculateBMI();
    if (bmi > 0.0) {
        oss << "\n【身体质量指数】\n";
        oss << "  BMI: " << std::fixed << std::setprecision(1) << bmi
            << " (" << getBMICategory() << ")\n";
    }

    // 最新数据快照
    if (!vitals.empty()) {
        const auto& latest = vitals.back();
        oss << "\n【最新体征】" << timePointToIso(latest.timestamp) << "\n";
        if (latest.heartRate)  oss << "  心率: " << *latest.heartRate << " bpm\n";
        if (latest.steps)      oss << "  步数: " << *latest.steps << " 步\n";
        if (latest.sleepHours) oss << "  睡眠: " << *latest.sleepHours << " 小时\n";
        if (latest.weightKg)   oss << "  体重: " << *latest.weightKg << " kg\n";
    }

    if (!bps.empty()) {
        const auto& latest = bps.back();
        oss << "\n【最新血压】" << timePointToIso(latest.timestamp) << "\n";
        if (latest.systolic && latest.diastolic)
            oss << "  " << *latest.systolic << "/" << *latest.diastolic << " mmHg\n";
    }

    // ASCVD (China-PAR)
    double ascvd = calculateASCVDScore();
    if (ascvd > 0.0) {
        std::string category = ASCVDCalculator::getRiskCategory(ascvd);
        oss << "\n【心血管风险 (China-PAR)】\n";
        oss << "  10年 ASCVD 风险: " << std::fixed << std::setprecision(1)
            << ascvd << "% — " << category << "\n";
    }

    oss << "\n[提示] 接入 LLM 服务后将提供 AI 驱动的个性化解读。\n";
    oss << "============================================================\n";
    return oss.str();
}

// ============================================================
// 周期性健康报告（周报/月报 — 基于时段均值）
// ============================================================

std::string HealthManagerImpl::generateHealthReport(ReportPeriod period) const {
    using namespace std::chrono;
    auto now = system_clock::now();
    int days = (period == ReportPeriod::WEEKLY) ? 7 : 30;
    auto from = now - hours(24 * days);

    const char* periodLabel = (period == ReportPeriod::WEEKLY) ? "周报" : "月报";

    std::ostringstream oss;
    oss << "==================== 健康数字孪生报告 ("
        << periodLabel << ") ====================\n\n";
    oss << "报告周期: 近 " << days << " 天 ("
        << timePointToIso(from).substr(0, 10)
        << " 至 " << timePointToIso(now).substr(0, 10) << ")\n\n";

    // ---- 查询时段内各类型记录 ----
    auto vitals = getVitalsRecords(from, now);
    auto bps    = getBloodPressureRecords(from, now);
    auto labs   = getLabTestRecords(from, now);

    // ---- 辅助 lambda：从记录列表中提取可选数值 ----
    auto avgOptDouble = [](const auto& records, auto accessor) -> std::optional<double> {
        double sum = 0.0;
        int count = 0;
        for (const auto& r : records) {
            auto val = accessor(r);
            if (val) { sum += *val; ++count; }
        }
        return count > 0 ? std::optional<double>(sum / count) : std::nullopt;
    };

    auto avgOptInt = [](const auto& records, auto accessor) -> std::optional<double> {
        double sum = 0.0;
        int count = 0;
        for (const auto& r : records) {
            auto val = accessor(r);
            if (val) { sum += static_cast<double>(*val); ++count; }
        }
        return count > 0 ? std::optional<double>(sum / count) : std::nullopt;
    };

    // ---- 1. 数据概览 ----
    oss << "【数据概览】\n";
    oss << "  - 体征记录: " << vitals.size() << " 条\n";
    oss << "  - 临床检验: " << labs.size()   << " 条\n";
    oss << "  - 血压记录: " << bps.size()    << " 条\n";

    if (vitals.empty() && bps.empty() && labs.empty()) {
        oss << "\n该时段内暂无数据。\n";
        oss << "============================================================\n";
        return oss.str();
    }

    // ---- 2. 体征指标时段均值 ----
    if (!vitals.empty()) {
        oss << "\n【体征指标 — " << days << " 日均值】\n";

        auto avgHR = avgOptInt(vitals, [](const VitalsRecord& r) { return r.heartRate; });
        auto avgSteps = avgOptInt(vitals, [](const VitalsRecord& r) { return r.steps; });
        auto avgSleep = avgOptDouble(vitals, [](const VitalsRecord& r) { return r.sleepHours; });
        auto avgWeight = avgOptDouble(vitals, [](const VitalsRecord& r) { return r.weightKg; });

        if (avgHR)    oss << "  心率:     " << std::fixed << std::setprecision(0) << *avgHR    << " bpm\n";
        if (avgSteps) oss << "  步数:     " << std::fixed << std::setprecision(0) << *avgSteps << " 步/日\n";
        if (avgSleep) oss << "  睡眠:     " << std::fixed << std::setprecision(1) << *avgSleep << " 小时/日\n";
        if (avgWeight)oss << "  体重:     " << std::fixed << std::setprecision(1) << *avgWeight << " kg\n";

        // 最新腰围（非均值，腰围变化缓慢）
        for (auto it = vitals.rbegin(); it != vitals.rend(); ++it) {
            if (it->waistCm) {
                oss << "  腰围:     " << std::fixed << std::setprecision(1) << *it->waistCm << " cm (最新)\n";
                break;
            }
        }

        if (!avgHR && !avgSteps && !avgSleep && !avgWeight) {
            oss << "  (无量化指标数据)\n";
        }
    }

    // ---- 3. 血压时段均值 ----
    if (!bps.empty()) {
        oss << "\n【血压指标 — " << days << " 日均值】\n";

        auto avgSys = avgOptInt(bps, [](const BloodPressureRecord& r) { return r.systolic; });
        auto avgDia = avgOptInt(bps, [](const BloodPressureRecord& r) { return r.diastolic; });

        if (avgSys && avgDia) {
            oss << "  收缩压:   " << std::fixed << std::setprecision(0) << *avgSys << " mmHg\n";
            oss << "  舒张压:   " << std::fixed << std::setprecision(0) << *avgDia << " mmHg\n";
            oss << "  测量次数: " << bps.size() << "\n";
        } else {
            oss << "  (无有效血压数据)\n";
        }
    }

    // ---- 4. 临床检验（时段内最新值）----
    if (!labs.empty()) {
        oss << "\n【临床检验 — 时段内最新值】\n";
        const auto& latest = labs.back();
        oss << "  采样时间: " << timePointToIso(latest.timestamp).substr(0, 10) << "\n";
        if (latest.fastingGlucose)   oss << "  空腹血糖:   " << std::fixed << std::setprecision(1) << *latest.fastingGlucose   << " mmol/L\n";
        if (latest.totalCholesterol) oss << "  总胆固醇:   " << std::fixed << std::setprecision(1) << *latest.totalCholesterol << " mmol/L\n";
        if (latest.hdlC)             oss << "  HDL-C:      " << std::fixed << std::setprecision(2) << *latest.hdlC << " mmol/L\n";
        if (latest.ldlC)             oss << "  LDL-C:      " << std::fixed << std::setprecision(2) << *latest.ldlC << " mmol/L\n";
        if (latest.triglycerides)    oss << "  甘油三酯:   " << std::fixed << std::setprecision(1) << *latest.triglycerides << " mmol/L\n";
        if (latest.uricAcid)         oss << "  血尿酸:     " << std::fixed << std::setprecision(0) << *latest.uricAcid << " µmol/L\n";
    }

    // ---- 5. BMI ----
    double bmi = calculateBMI();
    if (bmi > 0.0) {
        oss << "\n【身体质量指数】\n";
        oss << "  BMI: " << std::fixed << std::setprecision(1) << bmi
            << " (" << getBMICategory() << ")\n";
    }

    // ---- 6. ASCVD (China-PAR) ----
    double ascvd = calculateASCVDScore();
    if (ascvd > 0.0) {
        std::string category = ASCVDCalculator::getRiskCategory(ascvd);
        oss << "\n【心血管风险 (China-PAR)】\n";
        oss << "  10年 ASCVD 风险: " << std::fixed << std::setprecision(1)
            << ascvd << "% — " << category << "\n";
    }

    // ---- 7. 趋势概览 ----
    oss << "\n【趋势概览】\n";
    for (auto recType : {HealthRecordType::BP, HealthRecordType::VITALS}) {
        auto report = analyzeTrendReport(recType, from, now);
        for (const auto& mt : report.metrics) {
            const char* direction = (mt.slope > 0.05)  ? "↑" :
                                     (mt.slope < -0.05) ? "↓" : "→";
            oss << "  " << mt.metricName << ": "
                << std::fixed << std::setprecision(1) << mt.average
                << " " << mt.unit << " " << direction
                << " (" << mt.count << "次)\n";
        }
    }

    oss << "\n============================================================\n";
    return oss.str();
}

// ============================================================
// AI 驱动健康报告（AI-First + 离线降级）
// ============================================================

std::string HealthManagerImpl::generateAIReport(ReportPeriod period) {
    int days = (period == ReportPeriod::WEEKLY) ? 7 : 30;
    std::string periodLabel = (period == ReportPeriod::WEEKLY) ? "周报" : "月报";

    // ---- 离线降级路径 ----
    if (!llmService_ || !llmService_->isConfigured()) {
        std::cerr << "[Backend] AI 顾问未配置，使用本地" << periodLabel << std::endl;
        return generateHealthReport(period);
    }

    // ---- AI 路径: 提取数据 ----
    using namespace std::chrono;
    auto now = system_clock::now();
    auto from = now - hours(24 * days);

    auto profileOpt = getUserProfile();
    auto vitals = getVitalsRecords(from, now);
    auto bps    = getBloodPressureRecords(from, now);
    auto labs   = getLabTestRecords(std::nullopt, std::nullopt);  // 全部检验

    double bmi = calculateBMI();
    std::string bmiCategory = getBMICategory();
    double ascvd = calculateASCVDScore();
    std::string ascvdCategory = ASCVDCalculator::getRiskCategory(ascvd);

    // 趋势摘要
    std::ostringstream trendSummary;
    for (auto recType : {HealthRecordType::BP, HealthRecordType::VITALS}) {
        auto report = analyzeTrendReport(recType, from, now);
        for (const auto& mt : report.metrics) {
            const char* direction = (mt.slope > 0.05)  ? "↑" :
                                     (mt.slope < -0.05) ? "↓" : "→";
            trendSummary << "  - " << mt.metricName << ": 均值 "
                         << std::fixed << std::setprecision(1) << mt.average
                         << " " << mt.unit << " " << direction
                         << " (" << mt.count << "次)\n";
        }
    }

    // 组装 Prompt
    std::string systemPrompt = LLMService::buildSystemPrompt(periodLabel);

    std::string userPrompt;
    if (profileOpt) {
        userPrompt = LLMService::buildHealthContextPrompt(
            *profileOpt, vitals, bps, labs,
            bmi, bmiCategory, ascvd, ascvdCategory,
            trendSummary.str(), days, periodLabel);
    } else {
        // 无用户档案时的降级 Prompt
        std::ostringstream fallback;
        fallback << "用户尚未创建健康档案。请生成一份简短的" << periodLabel
                 << "，说明需要先完善档案才能获得个性化分析。";
        userPrompt = fallback.str();
    }

    // 调用 AI
    std::cerr << "[Backend] 正在请求 AI 生成" << periodLabel << "..." << std::endl;
    std::string aiResponse = llmService_->chat(systemPrompt, userPrompt);

    // 检查是否为错误 JSON
    bool isError = (aiResponse.find("\"error\"") != std::string::npos &&
                    aiResponse.find("\"error\":") != std::string::npos);

    if (isError) {
        std::cerr << "[Backend] AI 请求失败，降级为本地" << periodLabel << std::endl;
        std::ostringstream fallback;
        fallback << "⚠️ AI 服务暂不可用\n\n"
                 << "以下为本地生成的" << periodLabel << ":\n\n"
                 << generateHealthReport(period);
        return fallback.str();
    }

    std::cerr << "[Backend] AI " << periodLabel << " 生成成功" << std::endl;
    return aiResponse;
}

std::string HealthManagerImpl::askHealthAdvisor(const std::string& userQuery) const {
    std::cerr << "[Backend] 收到 LLM 咨询: " << userQuery << std::endl;

    // 尝试提供数据上下文（展示 RAG 管道已就绪）
    auto vitals = getVitalsRecords(std::nullopt, std::nullopt);
    auto labs   = getLabTestRecords(std::nullopt, std::nullopt);

    std::ostringstream oss;
    oss << "[数据摘要] 当前已录入 " << vitals.size() << " 条体征记录、"
        << labs.size() << " 条检验记录。";

    if (!vitals.empty()) {
        const auto& v = vitals.back();
        if (v.heartRate) oss << " 最新心率 " << *v.heartRate << " bpm。";
    }

    oss << "\n\n[占位回复] AI 顾问功能将在 LLMService 接入后启用，届时可基于您的真实健康数据"
        << "提供个性化建议。您的问题: \"" << userQuery << "\"\n\n"
        << "免责声明：本回复仅供参考，不构成医疗建议。";

    return oss.str();
}

} // namespace health

// ============================================================
// 工厂函数
// ============================================================
std::unique_ptr<health::HealthManager> health::createHealthManager() {
    return std::make_unique<health::HealthManagerImpl>();
}
