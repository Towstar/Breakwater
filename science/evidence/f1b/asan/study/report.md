# F1b private advection comparison

Study: **completed**; accuracy: **synthetic_targets_unmet**.

Synthetic targets: 58/63 passed. Production qualification: not established.

| N512 case | Worst mean-L1 mg/L | Worst RMS mg/L | Worst maximum mg/L | Saved outlet peak mg/L |
|---|---:|---:|---:|---:|
| front_smooth.space.n512.cfl0.02 | 9.27032203e-05 | 0.000340279601 | 0.00597644254 | 0.998966095 |
| front_box.space.n512.cfl0.02 | 0.00882997274 | 0.0520190519 | 0.512499824 | 1 |
| scheduled_inlet.space.n512.cfl0.1 | 0.00362054967 | 0.0237628283 | 0.285262871 | 0 |
| scheduled_inlet.extended_space.n512.cfl0.1 | 0.00681024728 | 0.0329166284 | 0.319094979 | 0.834465965 |

Failed targets:

- front_box.space.n512.cfl0.02.first_bracket.0.1
- front_box.space.n512.cfl0.02.first_bracket.0.9
- scheduled_inlet.extended_space.n512.cfl0.1.first_bracket.0.9
- extended.sampled_peak
- extended.crossing_count.0.9

- Candidate supports prescribed-inlet nonnegative-flow pure advection only; endpoint slopes remain zero.
- Candidate refinement is coupled space/time; empirical time differences are not exact temporal errors.
- Final-cell averages approximate physical outlet concentrations; sampled peaks and bracket times retain observation limitations.
- Existing Lean propositions apply to the original stencil, not this candidate or IEEE/scheduler/accuracy correctness.
- All study targets are synthetic; neither completion nor target success authorizes production adoption or simulated controls.
- Original F1 time-contamination failures and original coarse A9 failure retain their original scope.
