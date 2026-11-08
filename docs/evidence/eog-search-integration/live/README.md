# Installed hosted search evidence — 2026-11-08

Attempt10 is the first completed **strong live-search rehearsal** in this
10-attempt sequence. It ran the installed production controller, capture, Python
provider/native evaluator, SQLite history, and hosted Nemotron transport. No fake
transport or fixed tool script was used; no controls were applied.

## What completed

- Model: `nvidia/nemotron-3.5-lightning-30b-a3b`.
- Compiled runtime: `a23fb7061e666cf76c6f3a15ad2390b5134504c6`; the checkout
  at launch was subsequent QML-only polish `e8210a4e4f5eebc78fc43b18fed25542a619c2b2`.
- Official mock dataset, historical cutoff `1791460800`
  (`2026-10-08T12:00:00Z`), all three current pump-failure horizons.
- Exact requested objective matched preparation, actual process arguments,
  running SQLite session, and exported archive.
- The model issued `search_plans`; its completed global search had a matching
  successful tool-result continuation. A preceding unsupported subset request
  remained archived, and the model recovered with an all-system search.
- The final selected the provider's retained plan. Separate `validate_plan`
  requests independently matched that exact plan/hash, cost, outcome and complete
  repair horizons; controller review eligibility was true before history retirement.
- A grounded final and new full session archive completed: exit0,
  elapsed221.9803213seconds,11hosted requests/replies,10model-issued calls and
  nine successful tools. All source input hashes remained unchanged; stderr empty.

The retained search compared no action, truck-only, well-only and mixed plans.
Its selected mixed plan had one truck trip and three well shut-ins:

| Retained response | Modeled total cost |
| --- | ---: |
| No action | 12120.031267547434 |
| Truck-only | 7574.8769677257305 |
| Well-only | 6566.078435513853 |
| Selected mixed plan | 4171.971701661354 |

Selected cost comprised3266.971701661354lost revenue and905truck cost; the
validated outcome was `avoids_shutdown`. These are synthetic model outputs.
The search evaluated100candidates, stopped at its candidate limit, and declared
`best_found`, `complete=false`, with26numerically unavailable candidates. It
does not prove global optimality, physical accuracy, hydraulic feasibility or
operating approval. Native method identity was obtained through the installed
ABI (`eog-native-seconds-0.1.1`); validation used
`eog-rational-validation-0.1.0`. Exact installed module/runtime hashes are included.

## References for the local demonstration

- Session: `427b4639-5bef-43d3-a642-ceb9b418f859`.
- Capture: `06f7c1f05c8f487f866fa42d7cc6f6a1f6b3e77c975e8ba4d0b36f5c1f902e30`.
- Dataset: `dbac10215001f7cfcee641073c33aeeaa8cee27805757012296dd2dc1f295539`.
- Search request: `04f179f5-fd1f-4681-a906-f417dbc6df0a`.
- Matching search tool result: `call-aeddf526-aca1-4fd4-9924-125d25c3cf17`.
- Selected plan: `plan-2a4e845f62701d88af46643368bf6628`.
- Plan hash: `0f2b27de0ac28a35d68eecf5ebb0422e5ca3a448fc7ffbf41a21659927980430`.
- Validation: `validation-247aeef27cbbbc0b6e36fde942e60450b255b8c7ecf401f03a0fb563ae3219b2`.
- Local full archive SHA256:
  `b8e655ec14f3152cea58f042022d1c845e66408312240514796b9f2a8a7b23cd`.

The full importable archive remains only in the local ignored
`out/search-live/canonical-10/session.eog-session.json`. These published JSON
projections are sanitized evidence summaries, **not** history-import files.
Historical imports do not restore actionable approval.

## All attempts and the harness correction

`attempt-ledger-01-through-10.json` records every outcome, including incomplete
strong-search attempts01–09. Attempts02 and07 passed the generic investigation
gate without search; they are not relabeled as strong-search passes. Attempts03,
04,08 and09 completed numerical searches/validation but lacked a grounded final.

The old PowerShell launcher accidentally overwrote its `Objective` parameter
using a case-insensitive local variable. Attempts01–07 therefore used their
recorded default objective, **not** the requested overrides. Earlier statements
that the model ignored explicit search wording are unsupported and retracted.
The fix uses a distinct variable and exact propagation guards from08onward.
All prior archives and indexed case files were retained unchanged.

Attempt09 ended with transport code `response_invalid_empty`; its historical
finish-reason field was not retained and remains **UNKNOWN**. New transport code
allows only explicitly stopped empty public replies to reach the existing
once-only strict JSON correction. Attempt10 did **not** use a JSON correction,
so it does not demonstrate full-loop hosted empty-reply recovery.

This is one new installed strong-search pass. It is not three new search passes,
a reliability benchmark, or qualification of every free-form model assertion.
The public final is retained separately from authoritative numeric evidence.
No credentials, authorization headers, hidden reasoning or private absolute
capture paths are published. No additional hosted run was launched.
