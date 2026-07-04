#pragma once

#include <QString>

/// @brief OmniHealth Profiler 统一 UI 主题色板
///
/// 所有对话框共用此色板，确保视觉一致性。
/// 颜色灵感来自医疗健康场景（蓝/绿/橙/红语义）。
namespace Theme {

// ---- 语义色（风险等级）----
inline constexpr const char* Green()  { return "#2e7d32"; }  // 正常/低危
inline constexpr const char* Orange() { return "#e65100"; }  // 临界/中危
inline constexpr const char* Red()    { return "#c62828"; }  // 高危/极高危
inline constexpr const char* Blue()   { return "#1565c0"; }  // 信息/结论
inline constexpr const char* Purple() { return "#7b1fa2"; }  // 追问/交互

// ---- 功能色 ----
inline constexpr const char* Primary()   { return "#1565c0"; }  // 主色调（标题、关键按钮）
inline constexpr const char* Accent()    { return "#e65100"; }  // 强调色（警示）
inline constexpr const char* Okay()      { return "#2e7d32"; }  // 良好状态
inline constexpr const char* Warn()      { return "#e67e00"; }  // 警告状态
inline constexpr const char* Danger()    { return "#c62828"; }  // 危险状态
inline constexpr const char* Muted()     { return "#888"; }     // 次要文字

// ---- 背景色（卡片）----
inline constexpr const char* CardWarn()  { return "#fff3e0"; }  // 风险卡片背景
inline constexpr const char* CardOkay()  { return "#e8f5e9"; }  // 行动计划卡片背景
inline constexpr const char* CardInfo()  { return "#e3f2fd"; }  // 结论卡片背景
inline constexpr const char* CardAsk()   { return "#f3e5f5"; }  // 追问卡片背景

// ---- 卡片左边框色 ----
inline constexpr const char* BorderWarn() { return "#e65100"; }
inline constexpr const char* BorderOkay() { return "#2e7d32"; }
inline constexpr const char* BorderInfo() { return "#1565c0"; }
inline constexpr const char* BorderAsk()  { return "#7b1fa2"; }

// ---- 通用样式片段 ----
/// 对话框基础 QSS
inline QString DialogBase() {
    return "QDialog { background: #fafafa; }"
           "QGroupBox { font-weight: bold; border: 1px solid #ddd; "
           "border-radius: 6px; margin-top: 10px; padding-top: 12px; }"
           "QGroupBox::title { subcontrol-origin: margin; left: 12px; "
           "padding: 0 6px; color: #333; }"
           "QTextBrowser { background: #fafafa; border: 1px solid #ddd; "
           "border-radius: 4px; padding: 8px; font-size: 13px; }"
           "QPushButton { padding: 6px 16px; border-radius: 4px; }"
           "QComboBox, QDateEdit { padding: 4px 8px; }";
}

/// 结构化报告卡片的 CSS 内联样式模板
/// @param bgColor   背景色
/// @param borderColor 左边框色
inline QString CardStyle(const char* bgColor, const char* borderColor) {
    return QString("background:%1; border-left:4px solid %2; "
                   "padding:12px 16px; margin-bottom:12px; border-radius:4px;")
        .arg(bgColor, borderColor);
}

/// 风险等级 → 语义色
inline const char* riskColor(const QString& category) {
    if (category.contains("极高危") || category.contains("高危")
        || category.contains("High Risk"))  return Red();
    if (category.contains("中危") || category.contains("临界"))     return Orange();
    if (category.contains("低危") || category.contains("正常")
        || category.contains("Low Risk"))   return Green();
    return Muted();
}

} // namespace Theme
