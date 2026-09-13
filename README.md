# git-progressive

**Status: functional end-to-end. Ollama, OpenAI, and Claude backends.**

A terminal tool that takes a git branch (PR branch, `master`) or a commit
range and uses an LLM to reorganize the diff into a **progressive**
sequence of commits — core change first, then side changes, then
features layered on, like onion layers — so a reviewer can read phase by
phase instead of facing one flat wall of diff.

Point it at `master` and it works the same way for onboarding: phase 1 is
the repo skeleton (entry point, configs, build files, API interfaces),
later phases layer in implementation, logging, architecture.

The tool never lets the LLM write or rewrite code — it only decides how to
group and order the *real* hunks from `git diff`, then applies them as
real commits on a new branch (rooted at the range's earliest commit). Code
correctness stays entirely in git's hands.

Progressive commits are slices, not a full re-partition of the diff — a
phase can carry part of a file's change and leave the rest for later,
like slicing a cake. Full-file rewrites per phase are the exception.

## Why

AI-assisted coding makes diffs bigger and faster to produce than they are
to review. This is a human-in-the-loop aid: use the LLM to restructure a
change for reading, not to judge or write it.

## Design

See [`docs/design/git-progressive.md`](docs/design/git-progressive.md) and
the [implementation plan](docs/design/git-progressive-plan.md).

Planner: builds a compact category **outline** one hunk at a time
(bounded LLM context regardless of diff size), writes it to `outline.md`
and re-reads it back off disk, then group/orders it into phases, writes
`plan.md` and re-reads that back too — the files are the real hand-off
between passes, not just audit snapshots. See design doc for detail.

## Build

```
cmake -S . -B build
cmake --build build
./build/git-progressive <branch-or-range>
```

C++17, CMake. No system-wide installs — dependencies (cpp-httplib,
nlohmann/json, doctest for tests) are fetched into `build/` via CMake
FetchContent.

Or via the `Makefile` wrapper: `make build`, `make dry-run RANGE=master`,
`make run RANGE=master`, `make clean`.

## Test

```
cmake --build build
ctest --test-dir build
```

Unit tests (diff parser, hunk dependencies, plan validation, outline
round-trip) plus an agent-loop integration test against a scripted
mocked `Provider` — no real LLM calls.

## Run

Default backend is a local [Ollama](https://ollama.com) server with a
tool-calling model pulled (default `qwen3:8b`):

```
ollama pull qwen3:8b
./build/git-progressive master --dry-run          # print the plan only
./build/git-progressive feature-branch            # create branch 'progressive'
./build/git-progressive feature-branch --branch-name review --model qwen3:8b-q4_K_M
```

OpenAI and Claude also work — API key via `OPENAI_API_KEY`/`ANTHROPIC_API_KEY`,
no local server needed:

```
export OPENAI_API_KEY=...
./build/git-progressive feature-branch --provider openai --model gpt-5.1

export ANTHROPIC_API_KEY=...
./build/git-progressive feature-branch --provider claude --model claude-sonnet-5
```

Every run is auditable: it prints the outline/plan/log file paths at
startup (default `/tmp/git-progressive`, override with `--audit-dir
DIR`) — the same directory every run, truncated at the start of each
one. `outline.md` and `plan.md` are overwritten with the latest
snapshot as the run progresses; `agent.log` records every model
response and tool call/result — `tail -f` it to watch the agent reason
live.

## Backlog

Visualization of progressive phases (step-by-step viewer) beyond plain
git commits — see design doc backlog.
