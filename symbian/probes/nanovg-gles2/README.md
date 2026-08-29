# Vendored NanoVG / OpenGL ES 2.0 gate

This probe compiles the license-preserving NanoVG snapshot in
`symbian/third_party/nanovg` and hosts it in a Qt `QGLWidget` backed by OpenGL
ES 2.0. It embeds the NIKINIKI font subset and icon through Qt resources.

The page exercises:

- the vendored NanoVG C renderer and GLES2 backend;
- stencil and antialias paths;
- font atlas creation and text rendering;
- PNG decoding and image textures;
- a scrolling card list and touch selection.

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-Probe.ps1 -Project nanovg-gles2 -Configuration debug
```

Passing compilation is only the first half of the gate. The SIS must render
correctly and remain responsive on a Belle phone.

Three target-only compatibility declarations are kept in the probe rather than
patching the vendored files: GCCE compiles `nanovg.c` as GNU C99, the missing
`GLchar` alias is supplied for Belle's otherwise GLES2-compatible header, and
`STBI_NO_THREAD_LOCALS` disables an ARM TLS relocation that Symbian
applications cannot link.
