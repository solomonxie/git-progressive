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

- [x] T2.1 Resolve input (branch name, or explicit `A..B` range) to a base commit + head commit — see `src/git/` — depends: T1.2
- [x] T2.2 Fetch unified diff for the resolved range (`git diff <base> <head>`) and per-commit metadata — see `src/git/` — depends: T2.1
- [x] T2.3 Create new branch rooted at the range's base commit — see `src/git/` — depends: T2.1

## Phase 3: Diff decomposition
Turn raw unified diff text into structured, addressable hunks (stable IDs,
file path, line ranges, content) — the unit the agent's tools and the
commit builder both operate on.

- [x] T3.1 Unified-diff parser → list of `Hunk{id, file, old_range, new_range, text}` — see `src/diff/` — depends: T2.2
- [x] T3.2 Hunk dependency detection: flag hunks in the same file whose
      context overlaps/depends on another hunk's lines — see `src/diff/` — depends: T3.1

## Phase 4: LLM provider & tool-calling protocol
Common message/tool types and per-provider clients that speak each API's
tool-calling protocol. OpenAI and Claude are genuine sibling backends, so
per-provider files are warranted.

- [x] T4.1 Message/tool types (`ToolSpec`, `ToolCall`, `CompletionRequest`,
      `CompletionResponse`) and the `Provider` interface — see `src/llm/provider.hpp` — depends: T1.2
- [x] T4.2 HTTP + JSON dependencies wired via CMake FetchContent (cpp-httplib, nlohmann/json) — see `CMakeLists.txt` — depends: T1.1
- [ ] T4.3 OpenAI provider (function/tool-calling API), API key via env var — see `src/llm/openai.cpp` — depends: T4.1, T4.2
- [ ] T4.4 Claude provider (tool-use API), API key via env var — see `src/llm/claude.cpp` — depends: T4.1, T4.2
- [x] T4.5 Ollama provider (local, no API key; `/api/chat` tool-calling, e.g. qwen3) — see `src/llm/ollama.cpp` — depends: T4.1, T4.2

## Phase 5: Agent loop runtime
Generic tool-calling loop mechanics, kept separate from what the
planner's tools actually do (Phase 6), so any future agent can reuse it.

- [x] T5.1 `Tool` interface (name, JSON schema, invoke) and a tool registry — see `src/agent/tool.hpp` — depends: T4.1
- [x] T5.2 `AgentLoop`: send request, dispatch returned tool_calls to
      registered tools, append results as messages, repeat until a
      terminal tool call or iteration cap — see `src/agent/loop.cpp` — depends: T5.1, T4.5

## Phase 6: Progressive planner agent
The core feature: an `AgentLoop` configured with hunk/file-reading tools
and a `submit_plan` tool whose local validation drives self-correction.
Depends on real hunks (Phase 3) and the agent runtime (Phase 5).

- [x] T6.1 System prompt: onion-layer grouping/ordering instructions,
      output contract (`submit_plan` schema: ordered `{title, rationale,
      hunk_ids[]}` phases) — see `src/planner/` — depends: none
- [x] T6.2 Read-side tools: `list_hunks`, `read_hunk`, `read_file`,
      `read_commit_messages` — see `src/planner/tools.cpp` — depends: T3.1, T5.1
- [x] T6.3 `submit_plan` tool: validates every hunk assigned exactly once
      and phase order respects hunk dependencies (T3.2); returns
      validation errors as the tool result for the model to self-correct — see `src/planner/tools.cpp` — depends: T3.2, T5.1
- [x] T6.4 Wire the planner as an `AgentLoop` instance (system prompt +
      tools from T6.1-T6.3), drive to a valid submitted plan or
      iteration cap — see `src/planner/planner.cpp` — depends: T5.2, T6.1, T6.2, T6.3
- [ ] T6.5 Large-diff handling: chunked/summarized `list_hunks` output
      when hunk count exceeds context budget — see `src/planner/` — depends: T6.2 — superseded by Phase 6b

## Phase 6b: Outline-then-plan pipeline (v2, addresses T6.5)
Two bounded LLM passes over a shared outline document, replacing the
single-session grouping decision in Phase 6 for diffs that don't fit one
context window; the outline pass and grouping pass are chained instead
of one big free-form ask. Implemented and manually verified end-to-end
against this repo's own history (`qwen3:4b-instruct-2507-q8_0`,
`--dry-run`): outline built hunk-by-hunk, a self-correction round-trip on
a dependency-order error, then a valid plan.

- [x] T6b.1 Outline data model + self-describing markdown serialization:
      item = category/title + pointers `{hunk_id}`, each pointer line
      rendered as `hunk_id — file:line-range` (looked up from the hunk
      table at render time, no commit_id — hunks come from one flattened
      range diff, not per-original-commit) plus a parser that
      reconstructs item id/title/hunk-id membership from that output — see `src/planner/outline.hpp` (`renderOutline`, `parseOutline`) — depends: T3.1
- [x] T6b.2 Outline-builder: iterate hunks in original diff order; per
      hunk, prompt with (current outline text, hunk diff) → append a
      pointer to a matched item or create a new one; reuses T6.2's
      read_file/read_commit_messages tools for extra context — see `src/planner/tools.cpp` (`ClassifyHunkTool`), `src/planner/planner.cpp` (`buildOutline`) — depends: T6b.1, T6.2
- [x] T6b.3 Grouping pass: outline.md is re-read back off disk (not the
      in-memory Outline) before this pass starts, so the file is the
      real hand-off contract between passes; ask the model to group/
      order outline items into phases (`submit_plan`-style: ordered
      `{title, rationale, item_ids[]}`) — see `src/planner/planner.cpp` (`planPhases`), `src/planner/tools.cpp` (`SubmitOutlinePlanTool`) — depends: T6b.1, T6.3
- [x] T6b.4 Deterministic expansion: phase → item_ids → hunk_ids is
      mechanical (no LLM); reuses the shared `validatePlan` (T3.2
      dependency check + coverage) on the expanded hunk list. plan.md
      gets the same disk round-trip as the outline — written on a valid
      submit_plan, then re-read back off disk as what's actually handed
      to the commit builder — see `src/planner/validate.hpp`, `src/planner/planner.cpp` (`renderPlanMarkdown`, `parsePlanMarkdown`), `src/planner/tools.cpp` — depends: T6b.3, T3.2
- [x] T6b.5 Auditability: every run writes a fresh timestamped directory
      (default under `/tmp`, override with `--audit-dir`) with
      `outline.md`/`plan.md` overwritten with the latest snapshot on
      every update, and `agent.log` with every model response/tool call/
      tool result as it happens; the CLI prints the three file paths to
      stdout before anything else runs — see `src/audit/`, `src/agent/loop.hpp` (`AgentLogFn`) — depends: T6b.2, T6b.3

## Phase 7: Commit builder
Turns a validated plan into real commits on the new branch. Last step
before the tool is end-to-end usable.

- [x] T7.1 Apply one phase's hunks (via `git apply --cached` on a synthesized
      patch) and create a commit with the phase's title/rationale as message — see `src/commit/` — depends: T6.4, T2.3
- [x] T7.2 Sequential application across all phases, stopping with a clear
      error (and partial branch left for inspection) on apply failure — see `src/commit/` — depends: T7.1

## Phase 8: CLI wiring & UX
Wire the phases above into the actual command a user runs.

- [x] T8.1 Arg parsing: range/branch input, `--branch-name`, `--provider`,
      `--dry-run` (plus `--model`, `--host` for Ollama) — see `src/cli/` — depends: T1.2
- [x] T8.2 End-to-end flow: git integration → decomposition → planner
      agent → builder, with progress output — see `src/main.cpp` — depends: T2.3, T3.1, T6.4, T7.2, T8.1
- [x] T8.3 `--dry-run` mode: print the planned phases without creating commits — see `src/cli/` — depends: T6.4, T8.1

## Phase 9: Testing
- [ ] T9.1 Unit tests: diff parser, hunk dependency detection — see `tests/` — depends: T3.1, T3.2
- [ ] T9.2 Integration test with a mocked provider driving the agent loop
      (scripted tool calls, fixed final plan) — see `tests/` — depends: T5.2, T7.2
- Verified manually instead (no `tests/` yet): ran the built binary
  end-to-end against this repo's own history with `qwen3:8b`, both
  `--dry-run` and real commit creation; confirmed the new branch's
  cumulative diff against its base is byte-identical to the original
  range's diff.

## Backlog
- [ ] B1 Visualize progressive phases (step-by-step diff viewer) — depends: T8.2
