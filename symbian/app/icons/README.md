# NIKINIKI icon assets

- `nikiniki.png` is the 256×256 RGBA application/UI icon, downscaled from the
  1254×1254 transparent PNG supplied by the project owner on 2026-08-28. It
  keeps the source artwork and alpha channel intact while avoiding a large
  decoded texture on memory-constrained Symbian devices.
- `nikiniki.svg` is a filter-free SVG-T launcher interpretation of the same
  artwork. Qt 4.7 qmake passes it to Symbian `mifconv` and packages the
  resulting scalable MIF for the Nokia Belle application grid.

Both files are NIKINIKI project artwork and are distributed under the
repository's GPL-3.0 license.
