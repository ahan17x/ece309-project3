# ECE 309 · Project 3 starter — Tool Use and the Tool Registry

Read `P3_spec.md` (in this folder) first. This file only covers setting up the repo.

## 1. Bring your P2 code

Copy four files from your P2 repo (details in `src/core/README.md`):

```
include/core/conversation.h      include/core/sentinel_scanner.h
src/core/conversation.cpp        src/core/sentinel_scanner.cpp
```

Do **not** copy `message.h`; the P3 one adds `Role::Tool`.

If your P2 code doesn't work, build against the instructor's reference instead:

```bash
cmake -S . -B build -DUSE_P2_REFERENCE=ON
```

## 2. Build and test (Linux / WSL2)

```bash
cmake -S . -B build              # ASan + UBSan are on by default
cmake --build build -j
./build/test_p2                  # P2 regression: must pass before you start
./build/test_loop                # provided tool-loop tests: pass once your 3 pieces work
./build/test_p3                  # your P3 tests
cd build && ctest --output-on-failure   # everything, plus the end-to-end goldens
```

To check that you compile with zero warnings, as the grader does, add
`-DMH_WERROR=ON` to the configure line.

## 3. Try the harness

```bash
./build/miniharness --no-tools --script scripts/greeting.script   # works on day 1
./build/miniharness --tools                                       # needs your ToolRegistry
./build/miniharness --script scripts/tools_demo.script            # needs all three pieces
./build/miniharness --script scripts/escape_attempt.script
```

`tools_demo` writes `workspace/notes/summary.txt`. That's fine: the e2e tests
use their own copy in `tests/e2e/workspace/`. To start the demo over, delete
that file.

## 4. What is yours

| File | Status |
|---|---|
| `src/tools/tool_registry.cpp` and the private section of `include/tools/tool_registry.h` | **you implement** (45 pts) |
| `src/tools/workspace.cpp` (`Workspace::resolve` only) | **you implement** (15 pts) |
| `src/tools/calculator_eval.cpp` (`calc_evaluate`) | **you implement** (10 pts) |
| `tests/p3/test_p3.cpp` (≥ 15 tests) | **you write** |
| `docs/design-log-p3.md` | **you write** |
| everything else | provided, do not modify |
