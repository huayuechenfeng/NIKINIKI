# NIKINIKI endpoint-boundary probe

The historical directory name is retained for traceability. The probe now
compiles the focused `symbian/include/network/bilibili_endpoints.h` list and
proves that the public NIKINIKI tree no longer requires the complete upstream
`wiliwili/include` directory.

The gate passes when GCCE builds and links the application and three
representative endpoints match their recorded values at runtime.

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-Probe.ps1 -Project upstream-api -Configuration release
```

The endpoint header records the pinned wiliwili commit and GPL-3.0 provenance.
