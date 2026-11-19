# Clean Windows release check

This check must be recorded on an actual clean Windows account/VM. It has not
been established by sanitizing processes on the developer's machine.

## Prepared handoff

The current prepared kit uses clean source `ad45d45`. Its immutable ZIP hashes
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
this checklist, the owner review packet and `clean-windows-result.template.json`.

Use a fresh Windows x64 VM for the acceptance below. Keep the networking state,
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
