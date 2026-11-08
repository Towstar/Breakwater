# F1 front/outlet/threshold study

Study: **completed**; accuracy: **characterized_not_qualified**.

| Family | Study | Error scope | Observed orders | Finest mean-L1 mg/L |
|---|---|---|---|---:|
| front_box | space | semidiscrete_spatial | 0.44471, 0.45749, 0.46767 | 0.0400178856 |
| front_box | time | time_at_fixed_mesh | 1.11034, 1.04811, 1.02277 | 0.00181191298 |
| front_smooth | space | semidiscrete_spatial | 0.79963, 0.88690, 0.94051 | 0.00914257122 |
| front_smooth | time | time_at_fixed_mesh | 1.04615, 1.02235, 1.01103 | 0.00138547334 |
| scheduled_inlet | extended_space | coupled_space_time | 0.31907, 0.36181 | 0.0410578453 |
| scheduled_inlet | space | coupled_space_time | 0.30748, 0.47972 | 0.0193030494 |

- No application-wide field, peak, outlet or threshold accuracy qualification is claimed.
- Scheduled-inlet refinement remains coupled space/time; no semidiscrete inlet oracle is supplied.
- Reference uncertainty is an observed disagreement/roundoff estimate, not a formal enclosure.
- Saved outlet peaks and crossing brackets do not resolve unsaved native states.
- The legacy coarse failure and refined 0.40191 mg/L front limitation retain their original scope.
