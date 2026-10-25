# Building Ashford's Halberd

Use the parent repository's [campaign build guide](../../docs/campaign-backend.md).
The native campaign requires an i686 POSIX GCC toolchain, the pinned decomp
checkout and its Katana/CRI declaration headers. The original compiler is not
used. Configure through tools/build-campaign.ps1; -Package runs checks before
creating the Windows player archive. The older x64 prototype uses a separate
build script.

No retail game data or SDK headers are packaged. The offline decomp adaptation
is retained in campaign/patches/offline.patch and applied idempotently during
configuration. Do not replace the pinned source with an arbitrary newer fork.
