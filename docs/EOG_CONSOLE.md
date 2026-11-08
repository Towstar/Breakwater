# EOG read-only desktop console

This increment adds a dataset-driven workspace alongside the existing editable
simulation workbench. It consumes only network.json, ops.json and telemetry.csv.
Detection, shutdown predictions, joint response search and EOG Nemotron tools are
not connected in this increment. Their reviewed interface is PR #33; numerical
implementation belongs to Person A.

## Open a dataset

After the documented Qt/desktop setup and build, launch the new executable:

```powershell
.\scripts\build.ps1 -Preset desktop
.\scripts\Start-Desktop.ps1 -Executable '.\out\desktop\desktop\breakwater.exe' -AppArguments @('--workspace', 'eog', '--eog-dataset', 'C:\path\to\dataset')
```

Plain interactive launch defaults to EOG. The workspace switch opens the preserved
workbench. Existing project, investigation, science, history, sensor and replay
startup operations select the workbench; contradictory EOG options are rejected.
Legacy bare smoke/screenshot checks still target the workbench.

Choose **Open dataset** to replace input atomically; cancellation or rejection
retains the previous dataset. **Dataset details** shows source identity, window,
operating metadata, warnings and the explicit analysis availability.

- All-system overview shows imported facilities, disposal sites and truck yards.
- Facility view shows wells, tank, pump and pipeline connections. Internal positions
  are schematic where individual coordinates are not supplied.
- Picking equipment selects its original-unit observations. Layout does not infer
  hydraulics or replace the travel times in operations data.
- The cutoff uses dataset time and hides later samples. Stale samples retain their
  raw values/timestamps; pump state is separate from freshness.
- Trends preserve extrema and real gaps within 8,192 display points. Tank percent
  guides come from warning/shutdown defaults plus 100% capacity. Negative and
  above-range measurements remain visible.
- EOG navigation does not edit the workbench, dispatch equipment or rewrite inputs.

## Render and CLI checks

Use --workspace eog, --eog-dataset, --smoke, --eog-report and --screenshot together
for explicit render evidence. --eog-facility selects a known facility for a
drill-down check. Reports bind the actual dataset/cutoff and require rendered scene
labels, a painted trend, tank guides and unchanged workbench state.

Report, screenshot and database paths must be distinct and separate from input
files and database journals. Legacy health/science report flags are rejected with
EOG startup input. Image/report writes use atomic replacement; an output hard-link
alias cannot rewrite an input's underlying bytes.

Local scoped acceptance before capture integration:
- Normal: 6/6 EOG CTests, 10.36 seconds.
- MSVC ASan: 6/6 EOG CTests, 49.14 seconds; no sanitizer finding in executed checks.
- Dataset suite: 24 Qt cases per mode. Actual executable CLI suite: 8 Python
  methods, including output collisions and hard-link input preservation.
- Four normal/compact overview/facility renders require real frame/trend readiness.
- CI policy: 15/15 after initializing the ignored local tools directory.

These are scoped console checks, not a fresh full combined application acceptance.
G2's separate full checks and later hosted runs retain their exact revisions.
Prebuilt Qt/Python dependencies are outside native ASan instrumentation coverage.

The hard-link regression needs native Windows filesystem capabilities. An
AppContainer sandbox denies link creation; retain that setup failure and execute
the check in a permitted native environment rather than silently skipping it.
ASan execution also needs the compiler runtime on process PATH.

Interactive QA observed the facility scene/trend, native chooser cancellation,
invalid replacement preserving prior data without exiting, and the workbench
switch. Automated folder text entry did not complete, so a new valid selection
through that native dialog is not claimed. Actual official full-week/mock parsing
was verified separately; no answer key is an application input.

## Next integration boundary

A causal capture producer is being added separately. It must retain raw bytes,
freeze the cutoff, verify source hashes and own its private directory for the job
lifetime. Production provider/client binding waits for Ethan's exact-contract
numerical acceptance. Imported or reopened analysis remains historical evidence.

