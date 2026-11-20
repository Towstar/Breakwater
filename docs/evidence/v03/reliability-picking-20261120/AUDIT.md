# Independent retained-evidence audit, November 20, 2026 UTC

Scope: three fixed EOG hosted rounds and two observed-origin GUI sessions.
A separate same-family agent inspected source contracts and retained records,
without building, operating the UI, changing evidence, or applying controls.
This checks internal consistency and numerical/source bindings; it is not an
authenticated execution attestation, independent model benchmark, owner review,
or release approval. `public_release_approved` remains false.

## EOG provenance and results

The [round manifest](hosted-rounds.json) preserves all twelve attempts. Each
round uses `operations` three times, followed by `renamed-topology` once.

| Round directory | Application code source | Recorded checkout | Result |
| --- | --- | --- | --- |
| `live-round-726a4da` | `726a4da4da640ec46ed0361047d02991641c57c2` | same | 2/4 |
| `live-round-7f6294d` | `7f6294d943c95642b62037df0678f83c9e1c2d87` | `efd82a6187ac9a5cbaba14e1d4415523fb2ed133` | 2/4 |
| `live-round-concise-29df04f` | `29df04f63a6af81671e1d223cb005c350a4a7289` | same | 4/4 |

The second checkout adds documentation only to its recorded application code.
All recorded checkouts were clean. Executable SHA-256 values, in table order:

```text
2b5adf2963e3c1e8926478475be08e23ce4053b9580d73b075ac4be99d9f9fe8
6b6dfceb285e1ba88910635841ae6ca049c5fd9e3a87ad700320b82269fe1e98
0d59767221a80d8a17cc4120e8391a6ce1f0d89037041b829ad9fd88ffcb7ef4
```

UTC run windows were 00:14:43–00:18:43, 00:38:22–00:42:11, and
00:46:19–00:48:50, respectively, on November 20. Each uses model
`nvidia/nemotron-3.5-lightning-30b-a3b`, a 4,096 output-token cap, the existing
ordinary `reasoning_budget=1024`, and unchanged application investigation caps.
The last round changes ordinary `enable_thinking` to false; JSON final turns
already used false.

| Round | Run | Status | Model requests | Completed calls | Reused validations | Actual numerical exchanges |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| 726a4da | 1 | Passed after one JSON-format correction | 8 | 6 | 0 | 5 |
| 726a4da | 2 | Passed | 9 | 8 | 3 | 3 |
| 726a4da | 3 | Initial transport timeout | 1 | 0 | 0 | 0 |
| 726a4da | 4 | Format correction, then grounding rejection | 10 | 8 | 3 | 4 |
| 7f6294d | 1 | Passed | 8 | 7 | 3 | 3 |
| 7f6294d | 2 | Passed | 13 | 12 | 8 | 3 |
| 7f6294d | 3 | Truncation correction, then grounding rejection | 6 | 4 | 0 | 4 |
| 7f6294d | 4 | Format correction, then grounding rejection | 11 | 9 | 4 | 4 |
| 29df04f concise | 1 | Passed | 13 | 12 | 6 | 5 |
| 29df04f concise | 2 | Passed | 13 | 12 | 6 | 5 |
| 29df04f concise | 3 | Passed | 13 | 12 | 7 | 4 |
| 29df04f concise | 4 | Passed | 13 | 12 | 7 | 4 |

Each archive's manifests and analysis envelopes passed local EOG schema/semantic
validation. The audit decoded all
36 retained input files in memory with Windows MSZIP, checked compressed/raw
SHA-256, sizes and source/capture identities, and matched numerical evidence
sample values against the captured CSV. It checked request/response/cutoff
bindings, plan hashes, costs, own independent validations, model metadata,
ordered call/result identities, counters, and accepted final references.
Copied artifacts were rehashed against the manifest and the original audited
artifacts. No credential/header/raw-reasoning fields or credential patterns were
found. All EOG archives are historical and nonactionable, with no operator records.

At cutoff `1794830400`, the operations dataset is
`5bfcdae6a07a3beb0875413980ef55b34bc5b955603bfbaa3deb41ed97cb11c0`, capture
`84745ddc89443d60c0effe6a163665e61884cb720598217baf4683b9206d7722`.
The renamed dataset is
`ce0aa8e4cd1871a3beb47f6779c280d4ee4004569cc5555da2ac59a0bbb180f6`, capture
`d4934a4536c9bd0e11079dc4975be69193190c2af9f01cf53fc5f7f3a06909fd`.
Both retain source observations beginning at `1794819600`.

All four concise-round selections cost $1,810 and have their own exact feasible,
available independent validations; no cheaper compatible retained evaluation
exists. The renamed run chooses an equal-cost alternative, preserving model
choice. The first round's second run instead completes with a $11,726 no-action
plan after never searching a cheaper response. This is grounded completion, not
evidence of effective optimization.

The first round's timeout lasts 60,206 ms, receives no HTTP status, and executes
no tools. The grounding failures in the other unsuccessful attempts retain only
a validated $11,726 baseline alongside seven cheaper compatible available
evaluations without their own validation. No final-selectable exact pair exists;
rejection is correct. Rejected final IDs were not retained, so their precise
mismatch cannot be inferred.

The concise round has 52 HTTP-200 requests, zero transport retries or format
corrections, 48 completed model-issued calls, and 26 reused validations. Every
run reaches the 12-tool ceiling and uses 13 model requests. Duplicated validation
has **not** been eliminated. The observed 4/4 versus earlier 2/4 is a small,
conditional scenario result, not a statistical reliability or comparative-model
benchmark. Previous unsuccessful attempts remain part of the evidence.

### EOG archive SHA-256

Paths are relative to this directory; each is named `run-N.eog-session.json`.

| Directory | Run | SHA-256 |
| --- | ---: | --- |
| `live-round-726a4da` | 1 | `b7fa75505a049e22d941d55b932a34eb8a8bcc4a8e06dab37e4b2e41b804f4a6` |
| `live-round-726a4da` | 2 | `4ba436489a12e2e9a6c9906e25df1a952c5ad75004a980d09fdc2b7c65f7aeed` |
| `live-round-726a4da` | 3 | `61326b305b2cae1e32fa59b70263600b51c607ca6e61decadd0a6d2e05679f73` |
| `live-round-726a4da` | 4 | `8d7e9516341f52cf395f9a06763d150a69cdc3559afb082e3a34947ca6a16b33` |
| `live-round-7f6294d` | 1 | `a2514e9e098e0bf5d8fce54f6013a44a441c5ade73a57b59ba274317c48bd091` |
| `live-round-7f6294d` | 2 | `d54703ce30f9cae443f60adcbbc04d10373f933e4a769a9c8f8b1d7dec54780c` |
| `live-round-7f6294d` | 3 | `208ba1493f9a70c0a07504ba4e9af4ff9ae589ac682f1fbb0e98373fd5a2b193` |
| `live-round-7f6294d` | 4 | `64fce1d49fdde2a345e6c0c372462a7e3e83c68263de05070b450828e8449fc5` |
| `live-round-concise-29df04f` | 1 | `edefdde0d5504a73c822cc205dc1df4cd70a2bbe2700cc5809c9604d7d3db946` |
| `live-round-concise-29df04f` | 2 | `a7c9e2e016002a2dfd89da985ca31dd3c436c3815a1a7fed66a64be6adc0d8c3` |
| `live-round-concise-29df04f` | 3 | `0abb5ee98fc7f61dd6077ff21b8710b442a3100d179e41ddcd8195987e7a1f6e` |
| `live-round-concise-29df04f` | 4 | `66bac3fb706ee0869c1b453a09940f65aa85910fdf84c47e8d65253eb5f88d3c` |

## Observed pump session

The [observed-case manifest](observed-cases.json) records both GUI cases and
distinguishes witnessed setup from independently checked archive facts.
The [pump archive](observed-pump-29df04f.session.json) was copied byte-for-byte
from `out/reliability-picking/observed-pump-29df04f.session.json`.
Session `2ede3623-9f9c-43d6-ad8b-f865c71b9aa9`; 174,636 bytes; SHA-256
`bb883e23d04a83a1723e10c75f9bb78d99ccca4b425637607a281a13a57899cb`.
This is a completed session-v2 `observed_origin` checkpoint from the source
29df04f GUI exercise, retaining full typed origin evidence. Its model metadata
and recommendation identify `source_mode=hosted`; the old audit's assumption
that selected observed runs must be local-native does not apply.

The audit validated configuration, snapshot and detection contracts against the
retained registry, recomputed configuration/design/effective identities, and
reconstructed the effective network and declared absolute uncertainty bounds
from all ten good, coherent required samples. Coverage is available with no
mapping issues. Capture `capture-817a8547-7899-45fd-8eda-d6cd4a6061ab` is at
event time `174.27052180000007`, expiring at `179.17199999990407`.
The frozen pump command is 44 m³/h; measured delivery is zero and typed feedback
is `delivery_loss`. The pump-delivery warning remains an unresolved diagnosis.
The balance detector is still warming up with only 20.03 seconds of history;
this session does not qualify a 30-second balance diagnosis. Generator seed 1337
is witnessed setup context, not independently established by this archive.

There are 13 successful correct-model HTTP-200 requests, no retries/failures,
and 12 matching model-requested/start/completed tool calls. The audit independently
recomputed all six stored runs' constant-flow costs and 1,452 trajectory values.
The selected run `ea0e2b4c-eaad-40f4-9908-06f756b311e3` has its own explicit
`validate_run` result `56c9e643-f74a-4aa1-bbf7-afe6f236159c`, cost 5,280 synthetic
units, and nominal minimum headroom `4.466034802435132` m³. It is the cheapest
of the retained feasible runs. The actual finite search evaluated 750 candidates,
screened out 724, and reference-checked five of 26 screen passes. These facts do
not establish global optimality, robust feasibility, chemistry or physical fault
diagnosis.

Four repeated `request_approval` calls all retain `applied=false` and
`pending_advisory_review`. There are zero approval records and no operating
authority. No credential/header/raw-reasoning findings were detected. `current`
describes the retained checkpoint; it cannot restore authority after import or
source retirement.

**Presentation finding:** untrusted model prose says the operator must “approve
and apply the staged controls,” despite the observed flow permitting advisory
acknowledgement only. The action guards remain intact. The UI and handoff must
keep acknowledgement-only wording authoritative; the prose is not authorization.

## Observed unavailable-inventory session

The [missing-preset archive](observed-missing-29df04f.session.json) was copied
byte-for-byte from `out/reliability-picking/observed-missing-29df04f.session.json`.
Session `a6ba03ac-1485-416a-9e56-3c5ab8149219`; 130,719 bytes; SHA-256
`047f40631e6dfb96d6ef004da0d411466d66f34a9077a104e95c0ab99720cca2`.
The witnessed setup used source 29df04f, preset `missing`, seed 1337, 1× speed
and paused acquisition. The seed and binary identity are not independently
attested by the archive.

Full configuration/snapshot contracts, semantic identity, source/revision/clock
bindings, sample-to-derived-state references and authority checks pass. Capture
`capture-ca38a5bc-dc17-4156-bd62-6802006c496e` is at event time
`76.47354339999993`. All ten raw samples remain retained. Tank A's last inventory
sample is at `59.03200000000652`, age `17.441543399993407` seconds, exceeding its
five-second maximum. It is explicitly marked `receiver_status=stale` while
retaining its original reported quality. Accordingly, `missing_sensor_ids` is
empty: the problem is unusable fresh coverage, not deletion of the old sample.
The mapper reports `missing_or_unusable_inventory`, excludes that derived
inventory, retains nine usable derived states, and publishes no effective
numerical network. Required samples also have 17 seconds of skew. Availability
is `unavailable`, with expiry equal to capture time. The investigation's retained
design network is not substituted for a valid observed numerical state.

The hosted model completes `no_validated_plan` with a null selection after 13
successful matching-model HTTP-200 requests, zero retries and 12 matched
read-only tool calls. Repeated observation/topology/catalog reads remain visible;
no forecast, candidate search, validation or approval tool executes. There are
zero runs, zero approvals and `actionable=false`. No secret/header/raw-reasoning
findings were detected. This qualifies safe completion for unavailable initial
inventory, not a successful numerical plan or infeasibility proof.

**Presentation findings:** the model's “infeasible by definition” claim is
unsupported. Unavailable state coverage is different from mathematical
infeasibility. Its explanation also conflates the detector's empty balance window
with the requirements for initializing a forecast. The authoritative blocker is
the unusable fresh inventory and incoherent observation set; a temporal balance
window is not universally required for a nominal volume forecast.

Both observed files were re-read after copying and matched their original bytes
and SHA-256. Normal, ASan, renderer, owner/license and clean-Windows release gates
remain separate and are not certified by this archive audit.
