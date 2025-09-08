# Contributing Guide / 貢獻者指南

感謝您願意為 SQALE 貢獻！此文件說明快速上手、開發流程、程式風格、測試與提 PR 的要求。請先閱讀根目錄的 `AGENTS.md` 與 `sqale/README.md`，兩者包含設計概觀與路線圖。

## 快速上手
- 需求：C11 編譯器（clang/gcc）。可選：LLVM 14+（啟用 `USE_LLVM=1`）。
- 建置：`cd sqale && make`（或 `make USE_LLVM=1`）
- REPL：`./build/sqale repl`
- 執行：`./build/sqale run examples/hello.sq`
- 產生 IR：`./build/sqale emit-ir examples/hello.sq -o out.ll`

## 開發流程
- 小步提交、保持 PR 聚焦；每個 PR 只解決一件事。
- 在 `sqale/examples/` 加入最小可驗證範例（.sq）說明行為；必要時新增 `tests/` 煙霧測試。
- 巨觀變更（語法／型別／IR）請先開 issue 討論，附上動機、設計草稿與替代方案。

## 程式風格
- C11；編譯旗標包含 `-Wall -Wextra -Werror -fno-strict-aliasing`。
- 縮排 2 空白，不使用 Tab；K&R 大括號風格。
- 檔案／函式：`snake_case`（如 `eval_program`, `rt_print`）；型別／列舉：`PascalCase`（如 `Type`, `ValueKind`）。
- 保持函式簡短；偏好小型輔助函式；避免不必要的全域狀態。

## 測試與驗證
- 範例即測試：在 `sqale/examples/` 放置可直接執行之程式，輸出須可人工比對。
- 巨觀功能請提供兩種驗證：REPL 互動與 `run` 執行檔案；如涉及 IR，附上 `emit-ir` 與 `clang` 的實際命令。
- 建議在開發階段啟用 Sanitizers：
  - `CFLAGS+=" -fsanitize=address,undefined -fno-omit-frame-pointer"`

## 提交與 PR 規範
- Commit 訊息用祈使句與範疇前綴：
  - 例：`parser: support arrow token`, `codegen: add add/sub lowering`。
- PR 需包含：動機、設計重點、測試步驟/輸出、相容性/風險、效能註記（若有）。

## 專題指引
- 巨觀功能（Macros/LLVM/Generics）請遵循 README 的路線圖；分階段送 PR。
- 第三方匯入：請在 `packages/` 下放範例套件與最小說明；示範 `SQALE_PATH` 使用方式。
- 安全性：避免不必要的 `malloc`/`free` 錯誤路徑；保持 GC 可追蹤物件的一致性；I/O 維持可預測邊界。

感謝貢獻！
# Contributing Guide

Thank you for considering a contribution to SQALE. This guide covers quick start, workflow, code style, testing, and PR expectations. Please also read `AGENTS.md` and `sqale/README.md` for architecture and the current roadmap.

## Quick Start
- Requirements: C11 compiler (clang/gcc). Optional: LLVM 14+ (`USE_LLVM=1`).
- Build: `cd sqale && make` (or `make USE_LLVM=1`)
- REPL: `./build/sqale repl`
- Run: `./build/sqale run examples/hello.sq`
- Emit IR: `./build/sqale emit-ir examples/hello.sq -o out.ll`

## Workflow
- Prefer small, focused PRs; one change per PR.
- Add a minimal, runnable `.sq` example under `sqale/examples/` that demonstrates behavior. Add a smoke test in `sqale/tests/` if helpful.
- For major changes (syntax / typing / IR), open an issue first with motivation, design sketch, and alternatives.

## Code Style
- C11; compile with `-Wall -Wextra -Werror -fno-strict-aliasing`.
- Indentation: 2 spaces, no tabs. K&R brace style.
- Files/functions: `snake_case` (e.g., `eval_program`, `rt_print`). Types/enums: `PascalCase` (e.g., `Type`, `ValueKind`).
- Keep functions small; prefer small helpers; avoid unnecessary global state.

## Testing & Verification
- Examples are tests: place self‑contained programs in `sqale/examples/`; outputs should be easy to verify.
- For larger features, show both REPL and file execution. If IR is involved, include `emit-ir` + `clang` steps.
- Recommended during development: sanitizers
  - `CFLAGS+=" -fsanitize=address,undefined -fno-omit-frame-pointer"`

## Commits & Pull Requests
- Commit messages in imperative mood with a scope prefix:
  - e.g., `parser: support arrow token`, `codegen: add add/sub lowering`.
- PRs must include: motivation, design notes, testing commands/output, compatibility/risks, and performance notes (if applicable).

## Feature Area Guidance
- Roadmap features (Macros / LLVM / Generics): follow the staged plan in README/ROADMAP; send incremental PRs.
- Third‑party imports: add a sample package under `sqale/packages/<name>` with a short usage example; document `SQALE_PATH` if needed.
- Security: avoid unsafe allocation/ownership paths; keep GC‑managed objects well‑formed; keep I/O bounded and predictable.

