# A10b native implementation regressions

Added after the post-A10a audit gate closed at `631f783` on
`codex/person-a-engine`. Only `engine/tests/transport_tests.cpp` changes executable
native source. The solver, diagnostics, epsilon guards, no-clipping policy,
timestep selection and public contracts are unchanged.

## Independent cases

Four new named cases extend the existing 21-case transport harness to **25**.
The previously delivered half-cell inlet-CFL case is strengthened in place.

| Case | Independent oracle / purpose |
|---|---|
| Open equality, N=2 and N=4 | `dx=1, dt=1/8, v=D=1, k=4`; sufficient budget exactly one. Rational impulse-response rows are first `[0,1/8]`, interior `[1/4,1/8,1/8]`, last `[1/4,1/4]`. Every returned value must be nonnegative. A separate N2 forced field `[2,4]`, inlet 3, source `[1,2]` gives `[7/4,7/4]` mg/L. |
| Closed equality, N=2 and N=4 | `dx=1, dt=1/4, v=0, D=1, k=2`; sufficient budget exactly one. Boundary rows `[1/4,1/4]`, interior `[1/4,0,1/4]`; every impulse response nonnegative. |
| Zero coefficients/data and pure decay | Both modes, N2: zero data stays zero. `k=2,dt=1/2` removes the entire old field at equality, allowing exact zero. Source `[2,0]` then gives `[1,0]` mg/L without requiring strict positivity. |
| Unequal open steps | Flow 1 then 1/2 at t=1/4; inlet knots 0,2,0 at t=0,1/4,3/4; source `[0,1]`. Steps 1/4 and 1/2 h each have inlet average 1. Old fields `[1,0] -> [1,1/2] -> [1,9/8]` mg/L; inlet/source/outlet ledger 500/750/125 mg. |
| Existing unsafe-inlet request | `dx=1,D=1,v=k=0,T=1/2,C=[1,0]`. Scheduler must take two safe steps, yielding `[1/18,5/18]`, instead of the unsafe single-step first-cell value `-1/2`. |

Expected impulse arrays are explicit rational values, not a copy of the solver's
flux or update loop. Cases assert one substep and check reported CFL against one in the
dyadic equality tests, check field/ledger diagnostics and exercise both boundaries
on the minimum mesh and with interior cells. Existing independent nonunit-geometry,
signed-mass and unequal closed source/decay cases remain intact.

## Actual verification

Windows x64 MSVC **19.51.36260.0**, Visual Studio 2026, CMake 4.3.1, Ninja,
C++20 RelWithDebInfo with `/fp:precise`; separate MSVC `/fsanitize=address` build.
Executed in the Person A worktree on November 7, 2026 (America/Chicago):

```powershell
./scripts/build.ps1 -Preset headless -EmbeddedDebugInfo
./out/headless/engine/breakwater_transport_tests.exe
./scripts/build.ps1 -Preset asan -EmbeddedDebugInfo
./out/asan/engine/breakwater_transport_tests.exe
```

The build helper initializes the MSVC x64 runtime in the same PowerShell process
and runs all configured native CTests. Each exit code was checked.

| Check | Result |
|---|---|
| Final normal build + CTest | 8/8 suites passed, 2.90 s |
| Direct normal transport harness | 25/25 named cases passed |
| Final separate MSVC ASan build + CTest | 8/8 suites passed, 37.00 s |
| Direct MSVC ASan transport harness | 25/25 named cases passed; no sanitizer report |

No failures/skips occurred. The first normal run, before strengthening the old
inlet case, also passed 8/8; the final results above use all delivered assertions.
Elapsed times are runtime observations, not benchmarks. Local build logs are
`out/a10b/native-final.log`, `asan-final.log`, `native-transport.log` and
`asan-transport.log`. Committed copies and tested source/executable hashes are
under [evidence/a10b](evidence/a10b/verification.json).

The [A10b exact-real proof](../proofs/FVM_NONNEGATIVITY.md) and these implementation
checks establish distinct claims. Neither an ASan pass nor these finite examples
prove compiled nonnegativity for all IEEE inputs or qualify front/threshold
accuracy. Full Python/worker/native parity was freshly checked in the preceding
audit; those boundaries are unchanged by this regression-only increment. No new
desktop, hosted model, learned training or accuracy experiment is claimed.
