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

Qt, Symbian SDK and device firmware components are external toolchain/system
dependencies and are not redistributed in this repository.
