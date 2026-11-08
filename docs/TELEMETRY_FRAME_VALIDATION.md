# Telemetry frame validation follow-up

Baseline: `1713cc48017363fdcc21d0aabaa7a6944f9a5c4f` (merged G2). This increment changes live validation work, with no changes to the G1 wire format, deadlines, size limits or stress-test counts.

The live worker previously revalidated the immutable network and complete sensor configuration for every frame. Configuration is already validated atomically before starting acquisition. The worker now validates each frame's structure against the cached physical identity and checks exact configuration-hash equality. The stream core continues to enforce the configured enabled sensor identities, measurement types and units, duplicate/sequence rules, epoch identity and clock policy. Invalid batches publish no partial measurements. Identity failures remain `frame_invalid`; invalid sensor/type/unit failures are now reported through the core's `stream_fault` path.

The duplicate-key walker previously created a separate JSON document for every key. Unescaped keys now use direct UTF-8 decoding after the existing whole-document syntax and UTF-8 checks. Escaped keys retain Qt JSON decoding, including alias detection. Duplicate, nesting-depth, UTF-8 and nonfinite-value protections are unchanged.

## Verification

Run from a compiler-enabled PowerShell using the existing configured build trees:

```powershell
cmake --build out/g2-normal --target breakwater_telemetry_contract_tests breakwater_telemetry_contract_check breakwater_telemetry_stream_tests breakwater_telemetry_lifecycle_tests --parallel 1
ctest --test-dir out/g2-normal -R '^(desktop.telemetry_contracts|contracts.telemetry|desktop.telemetry_stream|desktop.telemetry_lifecycle)$' --output-on-failure
cmake --build out/g2-asan --target breakwater_telemetry_contract_tests breakwater_telemetry_contract_check breakwater_telemetry_stream_tests breakwater_telemetry_lifecycle_tests --parallel 1
ctest --test-dir out/g2-asan -R '^(desktop.telemetry_contracts|contracts.telemetry|desktop.telemetry_stream|desktop.telemetry_lifecycle)$' --output-on-failure
```

Local results, 2026-11-08: MSVC 19.44.35219.0, Qt 6.12.0, Python 3.12.13.

| Check | Normal | AddressSanitizer | Cases / skips |
| --- | ---: | ---: | --- |
| Stream core | 4.21 s | 174.72 s | 31 / 0 |
| QProcess lifecycle | 25.39 s | 29.90 s | 27 / 0 |
| Native Qt contracts | 0.60 s | 22.67 s | 176 / 0 |
| Python/native comparison | 1.26 s | 7.35 s | 19 Python methods / 0 |
| CTest total | 31.48 s | 234.69 s | 4/4 passed in each build |

Focused regressions cover ordinary, Unicode and escaped-key aliases, malformed UTF-8 and surrogate keys, the existing nesting boundary, cached hash mismatches, disabled/unknown sensors, wrong reading types and units. Invalid sensor batches place the bad record last and verify that no preceding measurement leaks into the trace. The existing actual publisher/replay/command lifecycle and full stream stress cases also pass. No AddressSanitizer diagnostic was reported.

JUnit, build logs and Qt result files are retained locally under `out/frame-validation` and the two build trees. These ignored artifacts are not delivered as source. These measurements were collected while other local work was running, so they do not establish a stable throughput multiplier. The ASan stream run is close to its unchanged 180-second CTest cap; hosted runner variance still needs observation. No hosted CI or model-provider success is claimed by this local check.

## Hosted follow-up and instrumented harness budget

Hosted run `42475074847` passed five jobs, including the full normal desktop and
actual process lifecycle checks. The ASan desktop run passed 37/38 CTests; the
aggregate stream suite reached the fixed ledger stress case and exceeded its
180.07-second wall budget. Its clean synthetic-merge tree
`e12894009a9c2dd25d8ce57ae145ce4173ee65c9` equals tested source `ebc4aa9`.
The original failure and downloaded Qt logs remain retained.

Two independent same-family source reviews checked bounded ledger/reorder/trace
iteration and found no unbounded loop or growing-history scan. The 31-case suite
processes 33,024 readings plus 32,768 heartbeats and is functional stress coverage,
not a latency assertion. Only this suite's ASan CTest allowance is now 360 seconds;
normal remains 180. Protocol/command deadlines, stress counts and assertions stay
unchanged. Registration is checked separately in both configurations. A new full
hosted run must actually finish all 31 cases without skips before acceptance;
the larger finite allowance itself does not establish a pass or throughput claim.
