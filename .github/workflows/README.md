# GitHub Actions policy

The upstream wiliwili desktop/console build, Pages and WinGet workflows are
intentionally not enabled in the NIKINIKI repository. They target upstream
products and could publish artifacts under the wrong identity.

The Symbian³ application currently requires the legacy Qt SDK 1.2.1, Nokia
Belle SDK, GCCE 4.4.1 and a publisher-owned signing environment on Windows.
Until a reproducible isolated runner is available, release builds are made
locally with `symbian/Build-App.ps1` and verified on a physical Nokia 603.
Never add signing certificates, private keys, account cookies or device
endpoints to a workflow or repository secret unless a documented release
process explicitly requires it.
