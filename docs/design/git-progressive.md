# git-progressive: design doc

## Problem

Reviewing a large PR or an unfamiliar branch/repo as one flat diff dumps
every change at once — core logic, side effects, and unrelated cleanup
mixed together — with no reading order. Reviewers get flooded instead of
walked through a thought process. AI-assisted coding makes this worse:
agents now produce far larger diffs, far faster, than humans used to —
so the code a person has to *read and trust* increasingly outpaces the
code they wrote. This tool is a human-in-the-loop aid for that: use the
LLM to restructure the change for reading, not to write or judge it.

## Goals

- Given a branch (PR branch, master) or an explicit commit range, produce a
  new branch with the *same total diff*, but split into a **progressive**
  sequence of commits: core change first, then side changes, then
  additional features, in dependency order — onion layers, not flat.
- Each commit should read like a step in a lesson: reviewable on its own,
  building on the ones before it.
- Support both OpenAI and Claude as the LLM backend (user-supplied API key).
  Also supports local Ollama models (e.g. qwen3) — no API key, runs fully
  offline. Ollama is the only backend implemented so far; it's the default.
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

## Planner: single-shot vs agent loop

- **Single-shot prompt**: hunk summaries in one prompt → JSON plan out.
  Cheap, one request. But the model can only reason from hunk text/
  summaries — no way to pull in surrounding file context, related
  symbols, or original commit messages when judging "core vs side
  effect" — and an invalid plan needs a separate retry driver bolted on
  from outside the model's own reasoning.
- **Tool-calling agent loop** (chosen): the model drives a loop with
  tools — enumerate hunks, read a hunk's full diff, read surrounding
  file content, read original commit messages, and finally `submit_plan`.
  `submit_plan` runs local validation (full hunk coverage, dependency
  order) and returns errors as the tool result, so the model self-
  corrects inside the same loop rather than an external retry harness.
  More requests/cost, but this is what "LLM agents" in the original ask
  means: an agent that explores before committing to an answer, not one
  blind guess.

## Planner: two-pass outline-then-plan (chosen)

Grouping every hunk into phases in one session doesn't scale: context
blows up on large diffs, and it's one big free-form decision the model
has to get right all at once. Instead, the planner runs two bounded
passes over a shared **outline** document:

1. **Outline pass**: iterate hunks in original commit order. For each
   hunk, prompt the model with (current outline text, this hunk's diff)
   and ask it to either append a pointer (`file`, `hunk_id`) to a
   matching existing outline item, or create a new one — hunks come
   from one flattened range diff, not per-original-commit, so there's
   no commit id to track per hunk; `read_commit_messages` still gives
   the model original-commit context on demand. Output: a
   table-of-contents-style markdown outline — each item a category
   ("skeleton building", "feature: X", "infra: Y", ...) with pointers to
   every hunk that belongs to it. Context per call is bounded (outline
   size + one hunk), independent of total diff size.
2. **Grouping pass**: feed the completed outline (not raw diffs) to the
   model; ask it to group/order outline items into phases — same
   `submit_plan` contract as before: ordered `{title, rationale,
   item_refs[]}`.
3. **Deterministic expansion**: phases reference outline items, outline
   items reference hunk IDs — expanding phase → hunk_ids is mechanical,
   no LLM involved. Re-run the hunk-dependency check (T3.2) on each
   phase's expanded hunk list before handing off to the commit builder,
   since two outline items sharing a phase can still touch overlapping
   hunks out of appliable order.

This bounds per-call LLM context regardless of total diff size (answers
the "diff size vs LLM context" risk below) and narrows non-determinism to
two decision points (per-hunk classification, item grouping) — the final
hunk→phase mapping is always a deterministic expansion of the outline
and plan. Supersedes the single-session tool-calling loop above for the
*final grouping* decision; its read-side tools (`read_hunk`, `read_file`,
`read_commit_messages`) stay reusable — the outline pass can call them
too when a hunk's classification needs more context than its diff alone.

**Outline drift risk**: classification happens one hunk at a time, so
the model can conflate two distinct concerns into one item, or split one
concern across near-duplicate items, over a long run. Mitigate with a
small fixed set of top-level category prefixes (skeleton/feature/infra/
system/clean/robust) and always showing the model the full existing
outline so it has complete history to match against.

## Architecture

- `git` — resolves ranges, diffs, branch creation (shells out to `git`).
- `diff` — parses unified diff into hunks, detects same-file hunk
  dependencies.
- `llm` — provider clients (Ollama implemented; OpenAI, Claude planned)
  speaking each API's tool-calling protocol.
- `agent` — generic tool-calling loop: send messages+tools, dispatch
  tool calls, append results, repeat until a terminal call or iteration
  cap. Not planner-specific, so any future agent can reuse it.
- `planner` — the progressive-planning agent: system prompt + its tool
  set (list/read hunks, read file, read commit messages, submit plan
  with validation).
- `commit` — applies a validated plan's phases as real commits.

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
- **Agent loop cost/latency**: multi-turn tool use is slower and pricier
  than one prompt; needs an iteration cap and should degrade to "best
  plan so far" rather than looping indefinitely on a stubborn model.

## Backlog

- Visualization of the progressive phases: the CLI/branch output is the
  substrate, not the only front end. Once phases exist as structured data
  (not just commits), multiple presentations become possible — a
  step-by-step terminal walkthrough, a web diff viewer, an IDE plugin,
  a static HTML report for a PR. Not designed yet; v1 only needs the
  phases to exist as real, ordered commits.
