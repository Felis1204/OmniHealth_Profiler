#include "HealthManager.h"
#include "DataAccess.h"
#include "ASCVDCalculator.h"
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
// PIMPL 具体实现类
// ============================================================
class HealthManagerImpl : public HealthManager {
public:
    HealthManagerImpl()
        : dataAccess_(createDataAccess())
    {
        std::cerr << "[Backend] HealthManagerImpl 初始化开始" << std::endl;

        if (!dataAccess_->initialize("omnihealth.db")) {
            std::cerr << "[Backend] 警告: 数据库初始化失败，部分功能不可用" << std::endl;
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

    // ---- 风险计算 ----
    double calculateASCVDScore() const override;
    double calculateBMI() const override;
    std::string getBMICategory() const override;

    // ---- 趋势分析 ----
    TrendResult analyzeTrend(HealthRecordType type,
        TimePoint from, TimePoint to) const override;

    // ---- 统计摘要 ----
    std::string getStatistics(HealthRecordType type) const override;

    // ---- LLM 咨询 ----
    std::string generateHealthReport() const override;
    std::string askHealthAdvisor(const std::string& userQuery) const override;

private:
    std::unique_ptr<DataAccess> dataAccess_;

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
    return dataAccess_->insertRecord("user_profile", j.dump());
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
