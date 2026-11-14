---
name: branch-likelihood
description: Apply or review C++ [[likely]] and [[unlikely]] annotations using the user's shortcut, endpoint, iterator, and exception-path preferences.
---

# Branch likelihood annotations

Use this skill when adding or reviewing C++ `[[likely]]` and `[[unlikely]]`
attributes, especially in `include/popcount_ranges/*.hpp`.

## Preference

Treat annotations as a way to make the ordinary path the hot path and rare
shortcuts or exceptional cases cold. Preserve behavior; these attributes are
optimization hints, not correctness conditions.

- Mark sentinel and initial-state branches `[[unlikely]]` when they represent
  the two endpoints among a much larger set of ordinary combinations.
- Treat ordinary interior states as the expected path. An explicit `[[likely]]`
  can be used when it clarifies an important branch, but avoid redundant or
  contradictory hints.
- A zero-distance case such as `if (first == last)` can be `[[unlikely]]` when
  the compared positions are normally distinct.
- In iterator `operator+=` and `operator-=`, the `0`, `1`, and `-1` shortcuts
  can be `[[unlikely]]` when callers generally use `++`/`--` for single steps
  and these operators mainly handle arbitrary offsets.
- Mark invalid-input and iterator-misuse checks `[[unlikely]]`, including
  conditions that lead to exceptions. The reason is that valid use is expected;
  throwing is also expensive, but need not involve a system call.

## Check actual call patterns

Mathematical rarity among all possible values is useful evidence, but is not
enough by itself. Check how APIs invoke the branch:

- `begin()`, `end()`, `size()`, `--end()`, and reverse iteration deliberately
  touch endpoint values. A sentinel branch can therefore be hot in a particular
  helper even though endpoints are rare among all combinations.
- Iterator subtraction may compare equal positions; zero distance is not
  necessarily rare for every caller.
- Small `+=`/`-=` offsets may be common in generic random-access algorithms,
  even when direct `++`/`--` is available.
- Debug-only validation and exception checks only affect builds where those
  checks are compiled in.

Prefer the user's expected workload when known. Avoid guessing about
input-dependent branches and loops.

## Ask about uncovered cases

When a branch's situation is not covered by this skill, ask the user whether
to leave it unannotated or use `[[likely]]` or `[[unlikely]]`. Explain the
branch and its relevant call patterns, and wait for their decision. The user
may also provide a new reason for the choice; incorporate applicable new
reasoning into this skill for future cases. If an existing rule applies but
its workload assumptions are uncertain, check representative call patterns
or benchmarks rather than guessing.

## Review checklist

1. Identify the branch's normal and shortcut/error paths.
2. Verify whether the supposedly rare path is common in its actual API context.
3. Use `[[unlikely]]` for a cold condition's body and `[[likely]]` only for a
   well-supported hot body.
4. Keep related endpoint or exception checks consistent where appropriate.
5. Confirm annotations do not imply a workload assumption that the callers do
   not satisfy.
