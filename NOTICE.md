# NIKINIKI notices and provenance

NIKINIKI is a Symbian³ port and substantial platform rewrite derived from
[xfangfang/wiliwili](https://github.com/xfangfang/wiliwili), pinned for this
port at commit `88e5876bea9502d06f46a8656e3530684d3aaf7d` (v1.6.0, `yoga`).
NIKINIKI and the retained wiliwili-derived material are distributed under the
GNU General Public License version 3. See `LICENSE`.

The public product name is NIKINIKI. `wiliwili for Symbian³`,
`wiliwili_symbian`, the `wiliwili` C++ namespace, UID and settings keys remain
in selected internal or historical locations for upgrade compatibility and
traceability.

## Included third-party material

- NanoVG snapshot: Mikko Mononen and Adubbz; zlib-style license in
  `symbian/third_party/nanovg/LICENSE.txt`. The snapshot was taken from the
  borealis submodule pinned at `5f08b286f3df737f3321d2247a6fe633fcead03c`.
- QR Code generator: Project Nayuki; MIT license in
  `symbian/third_party/qrcodegen/LICENSE.txt`.
- Source Han Sans font and the NIKINIKI CJK subset derived from it: Adobe;
  SIL Open Font License 1.1 in `symbian/resources/font/LICENSE.txt`.
- PPSSPP-FFmpeg H.264-only build: FFmpeg and PPSSPP contributors; LGPL 2.1 or
  later. See `symbian/third_party/ppsspp_ffmpeg/`.
- `video-card-bg.png`: retained from the pinned wiliwili resources and covered
  by GPL-3.0 as part of this distribution.

`symbian/third_party/mongoose_compat` is a clean-room, project-local JSON
reader exposing only the small API surface used by NIKINIKI. No Mongoose source
is compiled or distributed as part of the application.

## Qt runtime installers

The following unmodified runtime installers are intentionally redistributed in
`prerequisites/` as the only binary-package exception in this repository. They
were extracted from the installed `Qt SDK for Symbian` Anna package:

| File | SDK source path | Size | SHA-256 |
|---|---|---:|---|
| `Qt-4.7.403-for-Anna.sis` | `sis/Symbian_Anna/Qt/4.7.4/Qt-4.7.403-for-Anna.sis` | 7,929,828 bytes | `E270767D363770B7CCFB3D3F1973BC9834F833BCCCD50A5441BDA82E6581B2CE` |
| `QtMobility-1.2.1-for-Anna.sis` | `sis/Symbian_Anna/QtMobility/1.2.1/QtMobility-1.2.1-for-Anna.sis` | 2,475,460 bytes | `312DE62AA8CA99AE5B460F58F5B2FDB3E45A33935525DACD327C98B44E701065` |

The installed SDK includes `LICENSE.LGPL` and `LGPL_EXCEPTION.txt` for its Qt
components. Those files identify the Qt source-license basis, but they do not
by themselves establish independent redistribution permission for these
historical SDK installer binaries. Their redistribution status must therefore
be confirmed against the applicable historical SDK terms before relying on
anything beyond this compatibility archive. Qt, the remainder of the Symbian
SDK, and device firmware are not otherwise redistributed here.
