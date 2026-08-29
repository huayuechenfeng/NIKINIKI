# NIKINIKI Symbian application target

NIKINIKI is the formal product name of the Belle application previously known
as `wiliwili for Symbian³`. The internal `wiliwili_symbian` executable target,
UID and settings namespace stay unchanged for upgrade and device-validation
compatibility. The application uses a C++03 retained view tree while
continuing to reuse the pinned upstream NanoVG implementation and selected
Bilibili API route constants.

Build from the repository root:

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-App.ps1 -Configuration debug
```

The M1 device gate is: application starts on the Nokia 603, ten recommendation
fixture cards scroll and select correctly, and the footer updates heap, free
memory, allocation-cell, and frame counters without a crash.
