# git-progressive

**Status: work in progress — skeleton only, not yet functional.**

A terminal tool that takes a git branch (PR branch, `master`) or a commit
range and uses an LLM (OpenAI or Claude) to reorganize the diff into a
**progressive** sequence of commits — core change first, then side
changes, then features layered on, like onion layers — so a reviewer can
read phase by phase instead of facing one flat wall of diff.

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

## Build

```
cmake -S . -B build
cmake --build build
./build/git-progressive <branch-or-range>
```

C++17, CMake. No system-wide installs — dependencies (once added) are
fetched into `build/` via CMake FetchContent.

## Backlog

Visualization of progressive phases (step-by-step viewer) beyond plain
git commits — see design doc backlog.
