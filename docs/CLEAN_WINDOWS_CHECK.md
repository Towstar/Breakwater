# Clean Windows release check

This check must be recorded on an actual clean Windows account/VM. It has not
been established by sanitizing processes on the developer's machine.

## Prepared handoff

Use the new kit's **CANDIDATE_SUMMARY.md**, `package.json` and `handoff.json` to
identify its actual application/source/ZIP hashes. Kit preparation checks those
identities; it does not establish CI, hosted qualification, clean-machine
acceptance or author consent. CANDIDATE_READINESS.md is the current preparation
and review packet. Both included RELEASE_REVIEW packets are historical
context tied to the revisions they name. Record current exact-head engineering
evidence and human review separately.

New kits use `acceptance_version: 2`. The pending template adds observed monitoring,
Connections/route focus and scene-focus acceptance below; every check starts at
`not_run`. Existing version-1 records and kits remain unchanged and describe only
their original coverage. Staging an older candidate with the current helper adds
the new checklist, not new application features or inherited acceptance. Record
an unsupported required path explicitly as not run with its reason/failure.

### Earlier v0.3 development handoff

The earlier expanded candidate uses application source `c71249e`.
Its private kit and exact ZIP/source/executable hashes are indexed by
[the v0.3 verification record](evidence/v03/final-verification.json) and
[dated owner packet](RELEASE_REVIEW_V03.md). Real laptop scale checks and
sanitized bundled-process checks passed; actual clean-machine acceptance is
still pending. Preserve that historical kit and its records.

### Earlier v0.2 handoff

The earlier prepared kit uses clean source `ad45d45`. Its immutable ZIP hashes
and pending gates are in the
[handoff record](evidence/portfolio/release-readiness/handoff-ad45d45.json) and
[candidate manifest](evidence/portfolio/release-readiness/candidate-ad45d45.json).
The local folder is `out/release-readiness/Breakwater v0.2.0 ad45d45 VM handoff`.
It is prepared, not an executed clean-machine result.

From a source checkout, stage an existing sealed candidate with the installed
Python interpreter (the helper uses only the standard library):

```powershell
python scripts/prepare_release_handoff.py --candidate 'C:/ABSOLUTE/CANDIDATE' --destination 'C:/ABSOLUTE/NEW/HANDOFF'
```

The helper verifies the Windows ZIP/executable checksums and every source ZIP
file against the source manifest, then copies the exact archives. Existing
destinations are never replaced. `handoff.json` records preparation only; owner,
license and clean-Windows review remain pending. The kit includes checksums,
this checklist, the current candidate summary/readiness packet, both historical owner packets and
`clean-windows-result.template.json`. No existing result is upgraded in place.

Use a fresh Windows 11 x64 VM for the acceptance below. Keep the networking state,
installed software and actual Windows display settings with its evidence. A VM
using software rendering is not subject to the developer laptop's FPS targets.

## Execute and retain actual results

1. Copy the candidate ZIP and its recorded SHA-256 to Windows x64 without the
   development SDK, Python, Torch, repository paths or NVIDIA key.
2. Verify the ZIP hash and extract it to a folder with spaces and Unicode in its
   path, for example `C:\Demo apps\Breakwater α`. Keep the source ZIP/notices with
   the candidate. Do not copy an old `.env`, database or user configuration.
3. Disconnect networking for the offline check. Double-click
   `Verify-Breakwater.cmd`. It uses bundled Python; neither PowerShell 7 nor an
   installed Python/Qt SDK is required. Retain the newly created sibling
   `verification-...` directory, its JSON, logs and screenshots.
4. Double-click `Start-Breakwater.cmd`. Run the Sandbox forecast and local search,
   compare trajectories, inspect review, restart and reopen actual saved history.
5. Load both bundled Operations examples and inspect their original units,
   cutoff, plan comparison and independent validation. Open the recorded Nemotron
   example while disconnected. It must remain historical and cannot authorize
   actions. A fresh hosted request must report missing credentials accurately.
6. Check 100%, 125%, 150% and 200% display scaling. Resize to the supported
   960×540 client minimum and verify close buttons, menus, scrollbars and keyboard
   focus. Do not infer a clean-machine result from screenshot-only tests.

### Observed monitoring and advisory boundaries

1. Open **Project > Local monitoring example**, then **Local monitoring**. Start
   the bundled synthetic publisher with a recorded preset and integer seed, without
   installing Python/Torch or supplying a model key. Check actual received samples,
   matching pause/resume/stop acknowledgements and source/epoch identity. The plant
   is synthetic/local and has no actuation interface.
2. Pause, then **Capture state**. Retain capture identity, sample references,
   coverage, calibration/uncertainty, frozen commands and expiry. Check an available
   coherent capture and a missing-coverage preset separately. Unavailable coverage
   must remain explicit and must not fall back to design-only numerical work.
3. Open **Forecast / advisory review**. Run a native forecast and **Local search**
   on the available capture. Compare compatible baseline/candidate trajectories
   and open **Review staged plan** to inspect the exact run, independent validation,
   source hashes, controls, units and margins. **Acknowledge advisory review** records
   review only: Sandbox controls, publisher commands and incident clearing must not
   change. No hosted request is needed for these offline checks.
4. Exercise capture expiry and source replacement/restart. A new epoch must retire
   old current-source eligibility; expired, replayed or retired evidence cannot
   regain advisory review authority. Completed trajectories remain historical.
   Restart the application and reopen actual retained observed evidence; imported
   or restored history must not become an actionable capture or applied plan.
5. Retain the actual observations, rendered status/review screenshots and failure
   messages. A no-plan result caused by missing coverage is a safety-boundary case,
   not evidence of numerical infeasibility or successful pump-fault planning.

### Connections and workspace framing

1. Select **Connections** in Network, filter by route/endpoint ID and select a
   route. Verify exact endpoints, configured rate and separate availability in the
   inspector and scene card. A direction arrow must not imply measured delivery.
2. Use Focus/F for the selected route, then Top and Fit. Verify both endpoints and
   labels are visible. Exercise pan/zoom and resize the navigator/inspector; Fit
   must restore framing without changing the project, controls or stored approval.
3. Toggle **View > Scene focus mode** or Ctrl+Shift+F outside text fields/dialogs.
   The scene gains the panel space; Escape restores the prior panel preferences.
   Repeat the minimum-size and real display-scale checks with the overlays visible.

Record Windows build, CPU/GPU/driver, display resolution/scaling, candidate/source
hashes, exact steps, failures, counts and restart behavior. Optional Science is a
separate installation/qualification. A missing Torch capability in the basic
bundle is expected, not an application failure.

Also exercise exact-plan simulated review/application, numerical-edit invalidation
and reset; save/open/export file-dialog cancellation; and keyboard focus through
menus, dialog close controls and scrolled inspectors. Use fresh local analysis for
the renamed dataset, rather than importing another capture's plans as current.

Fill a **copy** of the pending result template, naming the reviewer, VM, source
revision, exact archive hashes and generated verification report. Keep failures and
not-run checks explicit. The automated verifier continues to describe sanitized
processes even on a VM: actual VM identity and manual acceptance belong to this
separate record. Neither `prepared` nor an automated developer-process pass means
that clean-machine review or public release approval has occurred.

Publish no binary until the ownership/license/notices/source-delivery and clean
Windows gates in LICENSING and PORTFOLIO_RELEASE are satisfied.
