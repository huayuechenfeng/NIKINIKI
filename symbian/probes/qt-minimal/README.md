# Qt for Symbian minimal probe

This probe verifies the first toolchain gate only:

- QtCore and QtGui compile for the Symbian target;
- the generated application can be packaged as SIS/SISX;
- a maximized widget is shown on the device;
- touch/button input reaches Qt.

It intentionally does not depend on QML, networking, OpenGL ES, multimedia, borealis, or wiliwili application code.

From PowerShell at the repository root:

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-Probe.ps1 -Configuration release
```

If the SDK generates a different make target, pass it explicitly:

```powershell
.\symbian\Build-Probe.ps1 -MakeTarget release-gcce
```

Build the broader SDK module probe with `-Project qt-capabilities` after this minimal gate passes.

## Verified device-debug path

The debug build was deployed and debugged on a Nokia 603 through Public CODA over WLAN. Qt Creator used the Belle qmake at `C:\QtSDK\Symbian\SDKs\SymbianSR1Qt474\bin\qmake.exe`; a remote interrupt produced `SIGTRAP` and enabled all normal step controls.

Launch the configured project with:

```powershell
.\symbian\Launch-QtCreator.ps1
```

Keep CODA in the foreground before starting a debugging session. If the phone's WLAN address or CODA port changes, update Qt Creator's run settings first.
