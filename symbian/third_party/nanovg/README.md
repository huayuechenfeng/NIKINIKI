# NanoVG snapshot

These files are the minimal NanoVG/GLES2 source set required by NIKINIKI.
They were copied without functional changes from the borealis submodule at
commit `5f08b286f3df737f3321d2247a6fe633fcead03c`, originally reached through
the pinned wiliwili v1.6.0 tree.

NIKINIKI compiles `nanovg.c` as GNU C99 and defines
`STBI_NO_THREAD_LOCALS`/`NANOVG_GLES2`; those are build flags, not edits to the
vendored files. The included stb/fontstash headers retain their own notices.
See `LICENSE.txt` for the NanoVG license.
