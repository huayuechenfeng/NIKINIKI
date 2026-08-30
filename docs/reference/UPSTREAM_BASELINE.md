# wiliwili upstream baseline

Recorded: 2026-08-23

## Superproject

| Field | Value |
|---|---|
| Upstream | `https://github.com/xfangfang/wiliwili.git` |
| Upstream branch | `yoga` |
| Local port branch | `symbian-port` |
| Commit | `88e5876bea9502d06f46a8656e3530684d3aaf7d` |
| Commit subject | `Release version v1.6.0` |
| Commit date | `2026-04-25T13:56:59+08:00` |
| License | GPL-3.0 |

The NIKINIKI product repository records this upstream point as its immutable
porting baseline. Product code is maintained in NIKINIKI's own Git history.

The full upstream checkout is retained only in a separate sibling research
repository. The NIKINIKI repository does not contain those directories or
submodule working trees; it records this immutable commit, licenses and the
file-level reuse manifest instead.

## Pinned submodules

| Path | Commit |
|---|---|
| `library/MemoryModule` | `7739ba4b2d87395446bbdcad6ae8bf9131b4250b` |
| `library/OpenCC` | `ccae908834c2fe41ba02141fa5d0eef178a45080` |
| `library/QR-Code-generator` | `720f62bddb7226106071d4728c292cb1df519ceb` |
| `library/borealis` | `5f08b286f3df737f3321d2247a6fe633fcead03c` |
| `library/cpr` | `ef35a614f1feb6ba0d4de13b1950bcaf7faad060` |
| `library/libpdr` | `9dd3ab920940adac013a3cf40e3a5805d7e193e1` |
| `library/lunasvg` | `f924651b85cac47dbe15f51a4aa320461fc1d07b` |
| `library/mongoose` | `4328842400370f62d00b6a6b23f1cbe0cded4073` |
| `library/pystring` | `7d16bc814ccb4cad03c300dcb77440034caa84f7` |

Nested submodules pinned by borealis:

| Path | Commit |
|---|---|
| `library/borealis/library/lib/extern/SDL` | `15ead9a40d09a1eb9972215cceac2bf29c9b77f6` |
| `library/borealis/library/lib/extern/glfw` | `892256c3f630739fb02552544b8d83240883ec8a` |

All listed commits were explicitly fetched and checked out. A first shallow submodule clone did not contain several older pinned commits, so the pins were corrected individually instead of accepting the submodule tips.

## Host configuration probe

The upstream CMake configuration reaches dependency discovery successfully with Visual Studio 2019 and CMake 3.31.6:

```powershell
cmake -S . -B build-host-probe `
  -G "Visual Studio 16 2019" -A x64 `
  -DPLATFORM_DESKTOP=ON `
  -DDISABLE_OPENCC=ON `
  -DDISABLE_WEBP=ON `
  -DCMAKE_BUILD_TYPE=Release
```

Observed result:

- C and C++ compiler detection passed with MSVC 19.29.30145;
- Git version and the pinned wiliwili commit were detected;
- configuration stopped at the expected host dependency gap: `MPV_INCLUDE_DIR` and `MPV_LIBRARY` are absent.

This host MPV dependency does not block the Qt/Symbian minimal probe. It will be installed only when a complete desktop comparison build is needed.
