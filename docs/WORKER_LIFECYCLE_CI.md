# Worker lifecycle CI follow-up

PR87's original Windows normal job passed 87/88 CTest entries. The controlled
storage-worker class passed 120 Qt cases and failed at the retry PID observation
in cycle 8 of its 50-cycle lifecycle case. Qt reported the requested 10-second
observation timeout was too short at its boundary. The original artifact, exact
assertion and raw output are preserved in [the evidence](evidence/worker-lifecycle-ci/verification.json).

That fixture had both its whole-job deadline and PID observation set to ten
seconds. The job deadline starts before a retiring process is fully reaped and
the next interpreter can launch. Shared-runner scheduling, process retirement
and interpreter startup therefore compete with observation at the same boundary.
This evidence does not establish a production overlap, survivor or late result.

The fixture now uses a 60-second job deadline and a 30-second observation
threshold. Unexpected failure signals end the wait promptly, with cycle, phase,
public status/error, signal counts and PID diagnostics. All 50 cycles, original
Windows process handles, survivor checks, exact publication/cancellation counts
and the final late-publication check are preserved. Production deadlines and the
separate 750 ms deadline/destruction case are preserved.

At test-source commit `948d5bc0a8018d78c4cad7b6732b736c785420d0`:

- Normal: the complete class passes 121/121 cases, one CTest entry, 127.38 seconds.
- ASan: the complete class passes 121/121 cases, one CTest entry, 132.28 seconds.
- Independent same-family source review reports no consequential findings.

These were bounded builds of one existing test target, with a 4 GiB diagnostic
reserve. They do not replace full final-candidate qualification. The 8 GiB gate
for full builds, hosted qualification and packaging remains in force. ASan
instruments owned C++ targets; prebuilt Qt and Python/Torch remain outside that
instrumentation scope.

The earlier clean `fe9dd97` normal application graph also built successfully and
was reopened with the preserved private user project. The original approved
2.54 GiB cleanup completed with all recovery archives unchanged. The separately
approved 3.52 GiB inventory also completed: 13,013 files removed and all 104
recovery inputs verified unchanged, leaving about 9.9 GiB free at that checkpoint.
Final hosted cases, private packaging, clean-Windows acceptance, owner/licensing
review and ordered main integration remain pending.
