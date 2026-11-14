---
name: branch-likelihood
description: Apply or review C++ [[likely]] and [[unlikely]] annotations using the user's shortcut, endpoint, iterator, and exception-path preferences.
---

# Branch likelihood annotations

Use this skill when adding or reviewing C++ `[[likely]]` and `[[unlikely]]`
attributes, especially in `include/popcount_ranges/*.hpp`.

## Preference

Preserve behavior: `[[likely]]` and `[[unlikely]]` are optimization hints, not
correctness conditions. Base them on expected or measured call frequency, not
on the cost of a path. Prefer the ordinary path; use explicit `[[likely]]` only
when it clarifies a meaningful choice, and avoid redundant or contradictory
hints.

- Use `[[unlikely]]` for sentinel or initial-state branches when they are only
  the two endpoints among many ordinary interior states, and for invalid-input
  or iterator-misuse checks (including exception paths). Valid use is expected;
  throwing is expensive, but need not involve a system call.
- A zero-distance check such as `first == last` is unlikely only when positions
  are normally distinct. In iterator `operator+=`/`operator-=`, `0`, `1`, and
  `-1` shortcuts are unlikely only when these operators mainly handle arbitrary
  offsets and callers use `++`/`--` for single steps.
- A frequent path may be `[[likely]]` even if it does expensive work; in a hot
  loop, favoring common arithmetic can keep the instruction pipeline on the
  main work. If equivalent-probability branches differ in efficiency, prefer
  `[[likely]]` on the more efficient implementation to guide compiler treatment,
  not to claim different runtime probabilities.
- A loop's one-time exit may be `[[unlikely]]` after many ordinary iterations,
  favoring the repeated path; verify the exit is rare for the relevant inputs.

## Check actual call patterns

Mathematical rarity among all possible values is useful evidence, but is not
enough by itself. Check how APIs invoke the branch and account for the build:

- `begin()`, `end()`, `size()`, `--end()`, and reverse iteration deliberately
  touch endpoint values. A sentinel branch can therefore be hot in a particular
  helper even though endpoints are rare among all combinations.
- Iterator subtraction may compare equal positions; zero distance is not
  necessarily rare for every caller.
- Small `+=`/`-=` offsets may be common in generic random-access algorithms,
  even when direct `++`/`--` is available.
- Debug-only validation and exception checks affect only builds where those
  checks are compiled in.

Prefer the user's expected workload when known; otherwise, avoid guessing about
input-dependent branches and loops.

## Ask about uncovered cases

For an uncovered branch, explain its behavior and relevant call patterns, then
ask whether to leave it unannotated or use `[[likely]]`/`[[unlikely]]`; wait for
the user's decision. Incorporate any applicable new reasoning they provide into
this skill. If a rule applies but its workload assumptions are uncertain, check
representative call patterns or benchmarks instead of guessing.

## Review checklist

1. Identify normal and shortcut/error paths; verify their frequency in actual
   API call patterns, including endpoints, exceptions, and build configuration.
2. Use `[[unlikely]]` for a cold condition and `[[likely]]` only for a
   well-supported hot path; keep related checks consistent.
3. Confirm annotations do not assume a workload callers do not have.
