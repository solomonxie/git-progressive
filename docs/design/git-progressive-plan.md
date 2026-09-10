# git-progressive: implementation plan

See [design doc](git-progressive.md) for rationale.

## Phase 1: Project skeleton & build tooling
CMake project, directory layout, and a compiling no-op binary. Everything
else builds on this existing and compiling.

- [x] T1.1 CMake project + `git-progressive` executable target, C++17 — see `CMakeLists.txt` — depends: none
- [x] T1.2 Module directory skeleton (`src/cli`, `src/git`, `src/diff`, `src/llm`, `src/planner`, `src/commit`) with stub headers/classes — see `src/` — depends: T1.1
- [x] T1.3 README (WIP), LICENSE, .gitignore — see `README.md` — depends: none

## Phase 2: Git integration
Everything downstream needs real commit ranges and diffs. Shell out to the
`git` binary (avoid a libgit2 dependency for v1) behind a small wrapper.

- [ ] T2.1 Resolve input (branch name, or explicit `A..B` range) to a base commit + head commit — see `src/git/` — depends: T1.2
- [ ] T2.2 Fetch unified diff for the resolved range (`git diff <base> <head>`) and per-commit metadata — see `src/git/` — depends: T2.1
- [ ] T2.3 Create new branch rooted at the range's base commit — see `src/git/` — depends: T2.1

## Phase 3: Diff decomposition
Turn raw unified diff text into structured, addressable hunks (stable IDs,
file path, line ranges, content) — the unit the planner and builder both
operate on.

- [ ] T3.1 Unified-diff parser → list of `Hunk{id, file, old_range, new_range, text}` — see `src/diff/` — depends: T2.2
- [ ] T3.2 Hunk dependency detection: flag hunks in the same file whose
      context overlaps/depends on another hunk's lines — see `src/diff/` — depends: T3.1

## Phase 4: LLM provider abstraction
Common interface so the planner doesn't care which backend answers it.
OpenAI and Claude are genuine sibling backends here, so per-provider files
are warranted.

- [ ] T4.1 `Provider` interface (prompt in, structured JSON response out) — see `src/llm/provider.hpp` — depends: T1.2
- [ ] T4.2 HTTP + JSON dependencies wired via CMake FetchContent (e.g. cpp-httplib, nlohmann/json) — see `CMakeLists.txt` — depends: T1.1
- [ ] T4.3 OpenAI provider implementation (API key via env var) — see `src/llm/openai.cpp` — depends: T4.1, T4.2
- [ ] T4.4 Claude provider implementation (API key via env var) — see `src/llm/claude.cpp` — depends: T4.1, T4.2

## Phase 5: Progressive planner
The core feature: ask the LLM to group + order hunks into onion-layer
phases. Depends on both real hunks (Phase 3) and a working provider
(Phase 4).

- [ ] T5.1 Prompt template: hunk summaries in, phased plan out (JSON: ordered
      list of `{title, rationale, hunk_ids[]}`) — see `src/planner/` — depends: T3.1, T4.1
- [ ] T5.2 Plan validator: every hunk assigned exactly once, phase order
      respects hunk dependencies from T3.2, repair/retry pass on violation — see `src/planner/` — depends: T5.1, T3.2
- [ ] T5.3 Large-diff handling: chunked/summarized planning when hunk count
      exceeds context budget — see `src/planner/` — depends: T5.1

## Phase 6: Commit builder
Turns a validated plan into real commits on the new branch. Last step
before the tool is end-to-end usable.

- [ ] T6.1 Apply one phase's hunks (via `git apply --cached` on a synthesized
      patch) and create a commit with the phase's title/rationale as message — see `src/commit/` — depends: T5.2, T2.3
- [ ] T6.2 Sequential application across all phases, stopping with a clear
      error (and partial branch left for inspection) on apply failure — see `src/commit/` — depends: T6.1

## Phase 7: CLI wiring & UX
Wire the phases above into the actual command a user runs.

- [ ] T7.1 Arg parsing: range/branch input, `--branch-name`, `--provider`,
      `--dry-run` — see `src/cli/` — depends: T1.2
- [ ] T7.2 End-to-end flow: git integration → decomposition → planner →
      builder, with progress output — see `src/main.cpp` — depends: T2.3, T3.1, T5.2, T6.2, T7.1
- [ ] T7.3 `--dry-run` mode: print the planned phases without creating commits — see `src/cli/` — depends: T5.2, T7.1

## Phase 8: Testing
- [ ] T8.1 Unit tests: diff parser, hunk dependency detection — see `tests/` — depends: T3.1, T3.2
- [ ] T8.2 Integration test with a mocked LLM provider (fixed plan response) — see `tests/` — depends: T4.1, T6.2

## Backlog
- [ ] B1 Visualize progressive phases (step-by-step diff viewer) — depends: T7.2
