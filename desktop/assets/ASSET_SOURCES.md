# Desktop artwork

`breakwater-logo.svg` is Lucas's supplied `breakwater-white-vector.svg`, copied
without changes. SHA-256:
`db1284fe6f6e3bb4845f16d99554722ae2a7e8ce47be85ef8354e430dcc21840`.
The supplied PNG is reference artwork; it is not used as a background or redrawn.

`breakwater.ico` exports only the supplied SVG's `icon` group, without its
wordmark, on a graphite rounded tile for contrast in Windows light/dark surfaces.
It contains 16, 20, 24, 32, 40, 48, 64, 128 and 256 pixel PNG frames. Regenerate
it with `scripts/Generate-WindowsIcon.cpp`, linked against the pinned Qt
Core/Gui/Svg libraries, using `desktop/assets/breakwater-logo.svg` as input.
For MSVC use `/std:c++20 /MD /Zc:__cplusplus`; pass the logo path, ICO output
path and an optional PNG preview path to the resulting generator executable.
The original supplied SVG remains unchanged. The ICO is embedded both as a
native Windows executable resource and as the running application's window icon.

Action icons come from Lucide, exact revision
`53fab88ff9d1b48f4704b5b5b560d6b4d560bf4c`, under the included
`LUCIDE-LICENSE.txt` (ISC and the retained Feather/MIT notices).
Source: https://github.com/lucide-icons/lucide/tree/53fab88ff9d1b48f4704b5b5b560d6b4d560bf4c/icons

Scene controls additionally use `house`, `maximize`, `focus`, `layers`, `pencil`
and `mouse` from that same pinned revision and license.

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

The portfolio site's gravel SVG texture, service lane, concrete pads, tank
containment curbs, pipe supports and code-built industrial assemblies are original
repository artwork. They use Qt geometry and locally authored vector shapes, with
no downloaded model, texture pack, aerial imagery or external font. They are
schematic presentation; placement supplies no numerical hydraulic geometry.
