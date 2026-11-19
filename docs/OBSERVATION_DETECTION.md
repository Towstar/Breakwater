# Bounded local volume detection

`bounded-volume-monitor-0.1.0` runs on the acquisition worker. It consumes mapped
measurements and a separate frozen command catalog. It does not consume the
publisher's preset, seed, hidden fault truth or numerical future state.

Pump delivery symptoms compare fresh measured delivery and typed availability
feedback against commands. Three distinct relevant sample updates activate;
five measurements within half the bound clear. Unrelated channels and repeated
heartbeats cannot manufacture a debounce streak. Acknowledgement is a separate
operator record and does not clear an event.

For the bundled local plant, the explicitly declared temporal model permits a
30-second pre-assimilation inventory residual. Its absolute threshold includes
both endpoint inventory/calibration bounds, integrated flow uncertainty,
acquisition-age and clock uncertainty, per-channel transition quadrature and
floating-point roundoff. Opposing delivery transitions are bounded separately.
The residual is computed before any observed inventory initializes a forecast.
Generic external publishers and legacy replay do not automatically inherit this
temporal assumption; their balance status is explicitly unavailable.

Coverage gaps discard the balance window and break pending streaks. They do not
clear active events or establish a healthy plant. Network/configuration/revision,
source, epoch and mode changes retire dependent state. Event IDs remain stable
through updates, acknowledgement and clearing. Events report symptoms with an
unresolved diagnosis, never a mechanical root cause or control authority.

Recent detector state is bounded to 4,096 points and 32,768 numerical scalars.
Changed events include pre-assimilation numerical witnesses for reconstruction.
The adapter emits transitions immediately and ongoing evidence at most once per
ten event seconds; it does not start a database job or model request per packet.
Unchanged readings are not reparsed on every heartbeat.

Local monitoring displays the window/coverage status, affected equipment,
onset/update times, measured metric, absolute threshold, explanation and explicit
acknowledgement. Durable v2 history retains incident witnesses and separate
acknowledgements. A frozen capture now supplies an explicit
[native forecast/advisory investigation](OBSERVED_FORECAST.md); detection alone
does not start a forecast, model request or control action.

Qualification uses independent affine healthy references for two distinct
topologies, ten held-out seeds per topology and one hour per seed. It reports
physical event updates and missing coverage separately. Analytical fault/recovery,
transition and debounce cases supplement the healthy study. This synthetic
qualification does not establish field detection accuracy.
