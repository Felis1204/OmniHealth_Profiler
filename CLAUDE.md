# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (from project root)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build all targets
cmake --build build

# Build specific targets
cmake --build build --target health_backend    # static lib only
cmake --build build --target Health_Manager_App

# Run the application
./build/frontend/Health_Manager_App.exe       # Windows
./build/frontend/Health_Manager_App            # Linux/macOS

# Clean rebuild
rm -rf build && cmake -B build && cmake --build build
```

Currently there are no tests or linters configured. When they are added (e.g., Google Test), run them via:
```bash
cmake --build build --target test
ctest --test-dir build
```

## Project Architecture

This is **OmniHealth Profiler**, a C++ desktop personal health management system following **MVC with strict frontend/backend separation**.

### Directory Layout

```
Health_Manager/
├── CMakeLists.txt              # Root: C++17, adds backend/include globally
├── docs/
│   ├── PRD.md                  # Product requirements & feature specs
│   └── SYSTEM_DESIGN.md        # Architecture, class design, data flow, DB schema
├── backend/                    # Model + Controller (static lib)
│   ├── CMakeLists.txt
│   ├── include/                # Public interfaces (contracts)
│   │   └── HealthManager.h     # Currently the only interface
│   └── src/                    # Private implementations (PIMPL)
│       └── HealthManager.cpp
└── frontend/                   # View layer (executable)
    ├── CMakeLists.txt
    ├── src/main.cpp            # Entry point, links to health_backend
    └── Forms/widget.ui         # Qt Designer UI form (not yet integrated)
```

### Key Design Principles

1. **Strict layer separation**: Frontend only accesses backend via `HealthManager.h` interface. Backend never contains UI code. Dependency: View → Controller → Model/Service (one-way).

2. **Interface contract pattern**: All backend logic is declared in `backend/include/`, implemented via PIMPL in `backend/src/`. Backend exposes a factory function `CreateHealthManager()` that returns `std::unique_ptr<health::HealthManager>`.

3. **Planned extensions** (from docs): HealthRecord class hierarchy (VitalsRecord, LabTestRecord, etc.), SQLite3 persistence via DataAccess, LLM API integration via LLMService, ASCVD risk calculator, ImGui or Qt GUI.

4. **Current state**: Skeleton with in-memory storage. Single `HealthRecord` struct with enum `HealthMetricType`. Basic CRUD + statistics implemented. No GUI yet (console test only).

### Coding Conventions

| Item | Rule |
|------|------|
| Classes | PascalCase |
| Methods | camelCase |
| Members | snake_case with `_` suffix |
| Constants | UPPER_SNAKE_CASE |
| Headers | `#pragma once`, Doxygen `/// @brief` for public APIs |
| Memory | `std::unique_ptr`/`std::shared_ptr`, no raw new/delete |
| Errors | Return `bool`/`std::optional` across modules, no cross-boundary exceptions |
| Standard | C++17 features (auto, lambda, enum class, optional, string_view) |

### Essential Reading Before Development

- **`docs/PRD.md`** — Business context and feature requirements
- **`docs/SYSTEM_DESIGN.md`** — Full architecture (MVC layers, planned class hierarchy, data flow, SQLite schema)

## Compilation & Testing Strategy

1. **Build on demand, do not blindly build**: During incremental development, do NOT automatically run a full build (`make`) after every edit. It's acceptable for code to be in a temporarily "incomplete" state.

2. **Milestone-triggered builds**: Only run `cd build && make` when:
   - Scenario A: You have fully implemented a complete business module (e.g., finished both .h and .cpp for DataAccess) and believe it's ready for delivery.
   - Scenario B: The user explicitly gives instructions to "test", "compile", or "verify".

3. **Error-handling principles (very important)**: If you encounter compilation errors, strictly follow these red lines:
   - Absolutely do NOT delete core design interfaces or drastically change the SYSTEM_DESIGN architecture to force compilation.
   - First check for basic syntax errors (missing semicolons, namespace errors) or missing `#include`.
   - If you encounter "Undefined reference" errors, first analyze whether the corresponding .cpp hasn't been written yet. If so, ignore the error — do not blindly patch it.
