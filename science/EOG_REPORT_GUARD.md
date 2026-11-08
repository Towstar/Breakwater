# Offline inspection report input preservation

Lucas's reproduced overwrite request in `docs/EOG_COORDINATION.md` applies to
`inspect_eog.py --report`, including reports of failed input analysis. The
production worker never invokes this offline CLI.

The CLI now resolves the destination and each of the three fixed source names
before analysis and outside its failure-report handler. A resolved collision or
existing hard-link alias exits with argument error 2, without a report, temporary
file or source modification. The atomic report writer uses the checked resolved
destination. Ordinary success/failure reports and historical cutoffs are preserved.
This is a preflight guard for ordinary local filesystem use, not an adversarial
filesystem race guarantee.

Executed on Windows with CPython 3.12.13 and site packages disabled:

```powershell
& C:\Projects\Breakwater\.tools\python\cpython-3.12.13-windows-x86_64-none\python.exe -S -m unittest discover -s science/tests -p test_eog_inputs.py -v
```

Final result: **14/14 methods passed**, zero failures or skips (3.214 s).
The new CLI subprocess checks preserve every source byte for all three names on
both valid and malformed input; relative `child/../` aliases and three real NTFS
hard-link aliases also reject without replacing either directory entry. Existing
ordinary success/failure report checks still pass.

The first sandboxed run passed 13 methods and errored at Windows hard-link
creation (`WinError 5`), before that probe reached the CLI. The same suite was
rerun with permission to create its temporary hard links; no test was skipped or
replaced with a mocked link. No official dataset was used or modified.

Only A-owned offline CLI/tests and this evidence note changed. Native physics,
production provider behavior, schemas and packaging are unchanged. Lucas owns
the decision to include any additional offline tools in distribution.
