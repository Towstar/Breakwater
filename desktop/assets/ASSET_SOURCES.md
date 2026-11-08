# Desktop artwork

`breakwater-logo.svg` is Lucas's supplied `breakwater-white-vector.svg`, copied
without changes. SHA-256:
`db1284fe6f6e3bb4845f16d99554722ae2a7e8ce47be85ef8354e430dcc21840`.
The supplied PNG is reference artwork; it is not used as a background or redrawn.

Action icons come from Lucide, exact revision
`53fab88ff9d1b48f4704b5b5b560d6b4d560bf4c`, under the included
`LUCIDE-LICENSE.txt` (ISC and the retained Feather/MIT notices).
Source: https://github.com/lucide-icons/lucide/tree/53fab88ff9d1b48f4704b5b5b560d6b4d560bf4c/icons

For direct Qt Image use, icon strokes change only from `currentColor` to
`#f3f3f3`. Native control icon tinting provides semantic states. Local names are
the upstream names except these documented mappings:

- `history.svg`: upstream `clock.svg`.
- `waves.svg`: upstream `droplets.svg` (water/source action).
- `trash-2.svg`: upstream `trash.svg`.
- `triangle-alert.svg`: upstream `triangle.svg` plus a matching two-stroke
  exclamation marker. The neutral outline family and stroke widths are retained.

All assets are bundled locally. No font, web runtime, remote image, or platform
icon theme is required. Logo proportions and transparency remain unchanged.
