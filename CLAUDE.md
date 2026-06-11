# ============================================================
# Health_Manager C++ 项目规约 (.clinerules)
# 此文件指导 AI 代码生成，确保代码质量和架构一致性
# ============================================================

1. 【架构分离原则】
   严格遵守 UI (frontend) 与 逻辑 (backend) 分离的架构。
   - frontend 只能通过 backend/include/HealthManager.h 中暴露的接口与数据交互。
   - frontend 不得直接操作任何数据存储、网络请求或第三方后端库。
   - backend 不得包含任何 UI 相关代码（如窗口创建、渲染循环、用户交互）。

2. 【现代 C++ 规范】
   优先使用现代 C++ 特性，符合 C++17/20 标准：
   - 使用 std::unique_ptr / std::shared_ptr 管理动态资源，严禁裸 new/delete。
   - 使用 STL 容器 (std::vector, std::map, std::unordered_map 等) 替代 C 风格数组。
   - 使用 std::optional 表示可能为空的值，避免空指针风险。
   - 使用 std::string_view 传递只读字符串参数。
   - 使用 auto 进行类型推导（保持代码简洁，但不过度滥用）。
   - 使用 lambda 表达式替代小型函数对象。
   - 使用 enum class 而非 C 风格枚举。

3. 【接口契约】
   任何新建的后端业务逻辑，都必须：
   - 先在 backend/include/ 下声明（接口/抽象类/数据结构）。
   - 再在 backend/src/ 下实现。
   - 实现类应隐藏于 .cpp 文件中（PIMPL 模式），对外仅暴露接口和工厂函数。

4. 【命名约定】
   - 类名：PascalCase（如 HealthManager）
   - 函数/方法名：camelCase（如 addRecord）
   - 成员变量：snake_case 后缀 _（如 records_）
   - 常量/枚举值：UPPER_SNAKE_CASE（如 HEART_RATE）
   - 文件名：
     - 头文件：PascalCase.h（如 HealthManager.h）
     - 源文件：PascalCase.cpp

5. 【头文件规范】
   - 所有头文件使用 #pragma once 防止重复包含。
   - include 顺序：本模块头文件 → 标准库 → 第三方库（每组间空行分隔）。
   - 不要在头文件中使用 using namespace 语句。

6. 【错误处理】
   - 后端接口返回 bool 或 std::optional 表示操作成败。
   - 不抛异常跨越模块边界（frontend/backend 之间）。
   - 使用 std::cerr 或日志系统记录错误信息。

7. 【注释与文档】
   - 公开接口（头文件中的类和方法）必须使用 Doxygen 风格注释（/// @brief, @param, @return）。
   - 复杂逻辑块需添加行内注释说明意图。
   - TODO 标记格式：// TODO(owner): 描述

8. 【CMake 构建】
   - CMake 最低版本 3.20。
   - C++ 标准设为 17 或 20。
   - 第三方依赖优先使用 FetchContent 而非 git submodule。
   - 目录结构变更时，同步更新 CMakeLists.txt。

9. 【代码审查要点】
   - 是否有内存泄漏风险？
   - 是否遵循 RAII 原则？
   - 接口是否清晰、职责单一？
   - const 正确性是否得到保证？

---
# 宏观上下文记忆
- **业务上下文**：在进行任何新功能开发前，请务必静默查阅 `docs/PRD.md`。
- **架构上下文**：在创建新类或划分模块前，请务必静默查阅 `docs/SYSTEM_DESIGN.md`。确保你的代码设计符合 MVC 规范和我们在文档中约定的核心逻辑。
---
# 编译与测试策略 (Compilation & Testing Strategy)

1. **按需编译，禁止盲目构建**：
   在日常的增量代码编写过程中，不要每次修改都自动执行全局编译 (`make`)。接受代码处于暂时的“不完整状态”。

2. **里程碑触发机制**：
   只有在以下两种情况下，才允许主动在终端执行 `cd build && make`：
   - 场景 A：你已经完整地实现了一个具体的业务模块（例如写完了 DataAccess 的 .h 和 .cpp），并认为可以交付时。
   - 场景 B：我（用户）在对话中明确对你下达了“测试”、“编译”或“验证”的指令时。

3. **报错冷静原则 (非常重要)**：
   如果在编译测试时遇到报错，请严格遵守以下红线：
   - 绝对不允许通过“删除核心设计接口”或“大幅度改变 SYSTEM_DESIGN 架构”的方式来让编译强行通过。
   - 优先检查是否是基础语法错误（少分号、命名空间错误）或遗漏了 `#include`。
   - 如果遇到“Undefined reference (未定义引用)”错误，先分析是否是因为对应的 .cpp 还没开始写，如果是，请忽略该报错，不要盲目修补。
---

