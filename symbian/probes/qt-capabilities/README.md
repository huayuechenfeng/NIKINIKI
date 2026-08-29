# Qt/Symbian capability probe

This probe verifies that the Belle target can compile and link the modules needed by the first porting experiments:

- QtNetwork;
- QtDeclarative/QML;
- Qt Mobility Multimedia (`QMediaPlayer`);
- QtOpenGL and the native OpenGL ES 2.0 headers.

Build it after entering the SDK environment:

```powershell
.\symbian\Build-Probe.ps1 -Project qt-capabilities -Configuration release
```

Successful compilation proves SDK/linker availability only. Hardware decoding, TLS compatibility, rendering and memory behavior still require tests on a Belle device.
