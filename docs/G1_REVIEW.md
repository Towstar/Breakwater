# G1 telemetry contract review and adoption

Person B publishes the shared interfaces. Person A supplied the numerical
requirements in `science/OBSERVATION_SEMANTICS.md` at commit
`9941859b202d804b024d788b1238d94166b33309`, now merged through PR #28. That written
guidance is recorded review input; it does not by itself approve this later wire
contract or authorize dependent G2 implementations.

## Physical review checklist

Review the exact G1 PR head against the A-owned guidance:

- Inventory: keep original value/unit and canonical m3. The 80,000 L example means
  80 m3; a 132 m3 observation against 130 m3 capacity remains 132, without clipping.
- Level: explicit versioned strictly increasing level-volume knots. A's `(0,0),
  (1,30),(2,80)` example maps 1.4 m to 50 m3. Out-of-domain values are retained but
  unavailable for mapping. A's conservative uncertainty example is 0.7 m3, not a
  standard deviation or fault probability. The contract does not implement this mapper.
- Flow/delivery: a source command of 52 minus curtailment 10 is 42 m3/h, while an
  independent reading of 40 remains 40. Changing a failed pump's command does not
  establish delivered flow, recovery or availability.
- Pump states: commanded-off, running feedback, delivery loss, unavailable and
  missing/unknown feedback retain distinct meanings. Missing feedback is not healthy.
- Concentration: native values are cell averages; point observations need an explicit
  supported operator. A's interval `[0.125,0.625] m` across the four example cells
  averages 2 mg/L. Preserve acquisition window/delay and physical pipe support;
  no effluent observation is inferred from zero flow or scene-marker coordinates.
- Coverage/uncertainty: independent measured balance needs endpoint inventory and
  integrated measured flows. A's residual/bound example is 0.5 m3/0.5 m3 and does not
  identify a leak. Commands used as substitutes are named assumptions. Redundant
  disagreement cannot silently select a last writer or reduce uncertainty by averaging.

The shared validators check contract structure, identities, admissible units and
metadata. Observation/calibration mathematics, physical residuals, detector thresholds
and synthetic plant dynamics remain Person A work after the adoption gate.

Measured and unit-converted derived values must bind to one matching source sensor,
quantity, attachment, unit and canonical value. Calibration/model-derived state uses
the explicitly inferred origin with source references and named assumptions; this
does not assert that G1 implemented or verified the numerical mapper. Semantic
snapshot validation requires the registered network/configuration/hash context.

## Adoption record

- Numerical guidance: **recorded**, Person A commit `9941859`.
- Exact G1 schema/DTO/validator implementation: reviewed and tested evidence belongs
  to the published G1 PR and its current head; see that PR for current check results.
- Person A acceptance of the exact G1 contract: **pending** until explicitly recorded
  in its PR conversation/review with the accepted commit or tree identity.
- Shared A/B adoption baseline: **pending** until both use that same accepted commit.

Independent agent review is implementation review, not Person A signoff. Neither
passing CI nor a merged earlier handoff substitutes for that signoff. Do not start
dependent G2 mapping/publisher/editor/ingestion work while adoption remains pending.

## Delivery boundary

G1 provides interfaces, offline validators and synthetic acceptance fixtures. The
project-v2 envelope is reserved for sensor-bearing documents; existing project-v1
parsing/writing remains intact and the current editor does not load/save sensor
objects. No acquisition process, device connection, detector, learned model or
automatic control application is delivered by this contract increment.
