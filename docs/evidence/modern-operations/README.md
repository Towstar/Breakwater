# Operations evidence

The records are copied byte-for-byte from actual local builds and tests. Paths in
raw tool output identify the development machine; no private user-project files,
credential values or manual user-workspace screenshots are included.

- `df404bb/`: both full configured builds and the original 3-probe/33-regression
  matrices. Both pass, but artifact inspection exposed condensed lower-content
  clipping. These records are not relabeled as final-source evidence.
- `1b16a88/`: both final full configured builds, extended 3-probe checks and
  10 affected Operations/QML regression entries per build, all passing. The eight
  negative-option process cases are separate from CTest counts.
- `reviews/`: bounded source/artifact reviews, with earlier checkpoints and their
  actual limits preserved. Same-family agent review is not formal human approval.
- `manifest.json`: byte hashes for the collected raw records and reviews. It
  excludes itself and this explanatory README.
- `1b16a88/gallery.json`: three unchanged final-source Qt PNG identities and
  source/app/client/DPR metadata. PNGs are in the linked portfolio image folder.

`probe-records.json` extracts the complete probe JSON from each retained
`last-test-*-probe.log`. CTest JUnit truncates long JSON lines; the full LastTest
copies are authoritative for observation/geometry detail. They record zero model
attempts and unchanged sandbox/review state. At minimum size with the dock open,
159 of 210 plot pixels are visible at once; scrolling reaches the full caption
and bottom axis. This does not claim the entire chart fits without scrolling.

Normal final executable SHA256:
`b5c8e073dbc5cfd585221a2f457dda011a43faa3718e0a8023278cbfac628f91`.
ASan final executable SHA256:
`1081993d7c12dd833cb91f18cff674f8cc6b76e3de57b67f8501ff9c456ff080`.

These are focused Windows development-machine checks. Clean Windows, additional
real display scales, dense arbitrary networks, new hosted rehearsals and release
owner/licensing gates remain separate. See [implementation and reproduction](../../OPERATIONS_WORKSPACE.md).
