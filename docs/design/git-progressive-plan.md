# git-progressive: implementation plan

See [design doc](git-progressive.md) for rationale.

## Phase 1: Project skeleton & build tooling
CMake project, directory layout, and a compiling no-op binary. Everything
else builds on this existing and compiling.

- [x] T1.1 CMake project + `git-progressive` executable target, C++17 — see `CMakeLists.txt` — depends: none
- [x] T1.2 Module directory skeleton (`src/cli`, `src/git`, `src/diff`, `src/llm`, `src/agent`, `src/planner`, `src/commit`) with stub headers/classes — see `src/` — depends: T1.1
- [x] T1.3 README (WIP), LICENSE, .gitignore — see `README.md` — depends: none

## Phase 2: Git integration
Everything downstream needs real commit ranges and diffs. Shell out to the
`git` binary (avoid a libgit2 dependency for v1) behind a small wrapper.

- [ ] T2.1 Resolve input (branch name, or explicit `A..B` range) to a base commit + head commit — see `src/git/` — depends: T1.2
- [ ] T2.2 Fetch unified diff for the resolved range (`git diff <base> <head>`) and per-commit metadata — see `src/git/` — depends: T2.1
- [ ] T2.3 Create new branch rooted at the range's base commit — see `src/git/` — depends: T2.1

## Phase 3: Diff decomposition
Turn raw unified diff text into structured, addressable hunks (stable IDs,
file path, line ranges, content) — the unit the agent's tools and the
commit builder both operate on.

- [ ] T3.1 Unified-diff parser → list of `Hunk{id, file, old_range, new_range, text}` — see `src/diff/` — depends: T2.2
- [ ] T3.2 Hunk dependency detection: flag hunks in the same file whose
      context overlaps/depends on another hunk's lines — see `src/diff/` — depends: T3.1

## Phase 4: LLM provider & tool-calling protocol
Common message/tool types and per-provider clients that speak each API's
tool-calling protocol. OpenAI and Claude are genuine sibling backends, so
per-provider files are warranted.

- [ ] T4.1 Message/tool types (`ToolSpec`, `ToolCall`, `CompletionRequest`,
      `CompletionResponse`) and the `Provider` interface — see `src/llm/provider.hpp` — depends: T1.2
- [ ] T4.2 HTTP + JSON dependencies wired via CMake FetchContent (e.g. cpp-httplib, nlohmann/json) — see `CMakeLists.txt` — depends: T1.1
- [ ] T4.3 OpenAI provider (function/tool-calling API), API key via env var — see `src/llm/openai.cpp` — depends: T4.1, T4.2
- [ ] T4.4 Claude provider (tool-use API), API key via env var — see `src/llm/claude.cpp` — depends: T4.1, T4.2

## Phase 5: Agent loop runtime
Generic tool-calling loop mechanics, kept separate from what the
planner's tools actually do (Phase 6), so any future agent can reuse it.

- [ ] T5.1 `Tool` interface (name, JSON schema, invoke) and a tool registry — see `src/agent/tool.hpp` — depends: T4.1
- [ ] T5.2 `AgentLoop`: send request, dispatch returned tool_calls to
      registered tools, append results as messages, repeat until a
      terminal tool call or iteration cap — see `src/agent/loop.cpp` — depends: T5.1, T4.3, T4.4

## Phase 6: Progressive planner agent
The core feature: an `AgentLoop` configured with hunk/file-reading tools
and a `submit_plan` tool whose local validation drives self-correction.
Depends on real hunks (Phase 3) and the agent runtime (Phase 5).

- [ ] T6.1 System prompt: onion-layer grouping/ordering instructions,
      output contract (`submit_plan` schema: ordered `{title, rationale,
      hunk_ids[]}` phases) — see `src/planner/` — depends: none
- [ ] T6.2 Read-side tools: `list_hunks`, `read_hunk`, `read_file`,
      `read_commit_messages` — see `src/planner/tools.cpp` — depends: T3.1, T5.1
- [ ] T6.3 `submit_plan` tool: validates every hunk assigned exactly once
      and phase order respects hunk dependencies (T3.2); returns
      validation errors as the tool result for the model to self-correct — see `src/planner/tools.cpp` — depends: T3.2, T5.1
- [ ] T6.4 Wire the planner as an `AgentLoop` instance (system prompt +
      tools from T6.1-T6.3), drive to a valid submitted plan or
      iteration cap — see `src/planner/planner.cpp` — depends: T5.2, T6.1, T6.2, T6.3
- [ ] T6.5 Large-diff handling: chunked/summarized `list_hunks` output
      when hunk count exceeds context budget — see `src/planner/` — depends: T6.2

## Phase 7: Commit builder
Turns a validated plan into real commits on the new branch. Last step
before the tool is end-to-end usable.

- [ ] T7.1 Apply one phase's hunks (via `git apply --cached` on a synthesized
      patch) and create a commit with the phase's title/rationale as message — see `src/commit/` — depends: T6.4, T2.3
- [ ] T7.2 Sequential application across all phases, stopping with a clear
      error (and partial branch left for inspection) on apply failure — see `src/commit/` — depends: T7.1

## Phase 8: CLI wiring & UX
Wire the phases above into the actual command a user runs.

- [ ] T8.1 Arg parsing: range/branch input, `--branch-name`, `--provider`,
      `--dry-run` — see `src/cli/` — depends: T1.2
- [ ] T8.2 End-to-end flow: git integration → decomposition → planner
      agent → builder, with progress output — see `src/main.cpp` — depends: T2.3, T3.1, T6.4, T7.2, T8.1
- [ ] T8.3 `--dry-run` mode: print the planned phases without creating commits — see `src/cli/` — depends: T6.4, T8.1

## Phase 9: Testing
- [ ] T9.1 Unit tests: diff parser, hunk dependency detection — see `tests/` — depends: T3.1, T3.2
- [ ] T9.2 Integration test with a mocked provider driving the agent loop
      (scripted tool calls, fixed final plan) — see `tests/` — depends: T5.2, T7.2

## Backlog
- [ ] B1 Visualize progressive phases (step-by-step diff viewer) — depends: T8.2
