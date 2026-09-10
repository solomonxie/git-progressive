# git-progressive: design doc

## Problem

Reviewing a large PR or an unfamiliar branch/repo as one flat diff dumps
every change at once — core logic, side effects, and unrelated cleanup
mixed together — with no reading order. Reviewers get flooded instead of
walked through a thought process.

## Goals

- Given a branch (PR branch, master) or an explicit commit range, produce a
  new branch with the *same total diff*, but split into a **progressive**
  sequence of commits: core change first, then side changes, then
  additional features, in dependency order — onion layers, not flat.
- Each commit should read like a step in a lesson: reviewable on its own,
  building on the ones before it.
- Support both OpenAI and Claude as the LLM backend (user-supplied API key).
- Work on a whole-repo range too (e.g. `master`): first commit = skeleton
  (entry point, configs, build files, API interfaces), later commits layer
  in implementation, logging, architecture changes — same progressive idea
  applied to "learn this repo fast."
- CLI-first (`git progressive ...`, git-subcommand style).

## Non-goals (for now)

- Not a merge/rebase collaboration tool — it produces a new read-only
  branch for review, not something meant to replace the original branch.
- Progressive commits do **not** need to cover 100% of the diff as one
  atomic slice each — like slicing a cake, a phase can carry part of a
  file's changes and leave the rest for a later phase. Full-file rewrites
  are the exception, not the rule, and are used only when a hunk can't be
  meaningfully split.
- No guarantee that every intermediate commit compiles or passes tests —
  that's a stretch goal, not a v1 requirement.
- No visualization UI yet — backlog (see below).

## Options considered

- **LLM rewrites the whole diff/patch text itself** — flexible, but LLMs
  are unreliable at emitting byte-exact unified diffs at scale; high risk
  of silently corrupting code.
- **Static/AST-based clustering only (no LLM)** — deterministic and safe,
  but can't judge "this is the core change vs. a side effect" the way a
  reviewer would narrate it; misses the pedagogical ordering that's the
  whole point of the tool.
- **Hybrid: LLM plans, git applies** (chosen) — decompose the diff into
  hunks locally (git does the diffing, so hunk text is always correct),
  ask the LLM only to *group and order* hunks into phases with titles/
  rationale, then apply each phase's hunks as a real commit via git
  plumbing. The LLM never emits code, only a plan (JSON: hunk IDs → phase).

## Decision

Hybrid approach. It keeps code correctness entirely in git's hands (hunks
come from `git diff`, are applied with `git apply`/`git commit-tree`) and
uses the LLM only for the part it's actually good at: judging what's core
vs. peripheral and choosing a teaching order. This bounds the blast radius
of LLM mistakes to "bad ordering," never "corrupted code."

## Risks / open questions

- **Hunk interdependency**: two hunks in the same file can conflict if
  applied out of original order (e.g. a later hunk's context lines assume
  an earlier hunk already landed). Need a dependency/ordering check before
  applying, with a fallback (merge hunks together, or reorder within
  git's tolerance) when the LLM's requested order isn't directly appliable.
- **Diff size vs. LLM context**: very large ranges may exceed context
  budget; likely needs hunk summarization or chunked planning for huge
  diffs. Open question, not solved in v1.
- **Non-determinism**: repeated runs may produce different phase splits.
  Acceptable for a review aid, but worth noting for users expecting
  reproducibility.
- **Compilability per commit**: not guaranteed in v1 (see non-goals).

## Backlog

- Visualize the progressive phases (e.g. rendered step-by-step diff view)
  so a human can walk through phases without checking out each commit.
