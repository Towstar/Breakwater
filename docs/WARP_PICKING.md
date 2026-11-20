# Software-renderer picking verification

The retained `out/v03/clarity-renderer-warp-asan.json` reported five of six
canonical assets pickable. It did not record which asset failed. Its claimed
ten-second sample contained only 6.44 seconds of frame intervals.

The previous verifier guessed points near each asset's layout origin. This is
not necessarily a point on geometry: treatment's pickable vessels are centered
23 units to either side, while its origin lies between them. The verifier also
changed the camera immediately before probing, without waiting for the renderer
to synchronize. These can produce a failed test even when visible geometry is
pickable; they do not establish a production picking defect.

Benchmark report version 2 uses the scene's visible pickable model centers,
projects them through `View3D.mapFrom3DScene`, and invokes unrestricted
`View3D.pick`. A hit must still name the expected asset, so an occluding asset
cannot create a pass. No geometry, pick masks or user interaction has changed.
The report retains failed asset IDs and attempted coordinates/hits for diagnosis.

The camera sweep follows elapsed time rather than the number of delivered timer
callbacks. A precise ten-second measurement starts after the actual warmup.
Picking waits for two rendered frames after the camera stops, with a bounded
timeout that fails visibly. Numerical identity, body picking, retained screenshot
and non-overlapping labels remain functional gates. Hardware FPS targets remain
reported separately; WARP has no FPS acceptance target.

`desktop.scene_picking_warp` runs the actual canonical scene and retains JSON,
PNG and process logs under the desktop build's `test-artifacts` directory. It
requires an identified software adapter, all six nearest-object picks, discovered
treatment bodies, a full sample and settled frames. Run it against both normal
and ASan builds, for example:

```powershell
ctest --test-dir <absolute-build-directory> -C RelWithDebInfo -R '^desktop.scene_picking_warp$' --output-on-failure
```

Use the existing `--scene-benchmark <new-absolute-json-path>` command for hardware
and dense-scene checks. Do not reuse an existing output path; evidence creation
refuses overwrites. The retained five-of-six report remains historical evidence.
Compilation and actual renderer acceptance must be recorded separately from this
source correction.

The first hosted normal run (44647353356, job 135997254199) passed all six body
picks and reported zero overlaps, but correctly culled Production A's label in a
1008 by 672 client area. The saved orbit placed its label above the navigation
hint; the artifact PNG and JSON identify a viewport fixture issue, rather than a
renderer picking failure. Benchmark setup now invokes the existing view-only Fit
for every fixture. It retains the same production culling and overlap policy.
The WARP regression explicitly reproduces that compact client area with
`BREAKWATER_BENCHMARK_COMPACT=1` and still requires six labels and six exact body
picks. Reports include positioned label rectangles and culled asset IDs.

The first fitted local normal compact run at `bb250d7` still culled Production A
at a 96-pixel toolbar inset and 26-pixel label height; ASan passed at a different
end angle. No failed result is superseded by that different view. The original
six placement attempts could all collide or clamp near the upper boundary while
free room remained horizontally. Two bounded wider horizontal attempts now run
before culling, preserving priority, the four-pixel gap, viewport bounds and
reserved controls. A fixed placement regression reconstructs this captured
compact layout and tests neighboring label widths without weakening the six-label
renderer check.
