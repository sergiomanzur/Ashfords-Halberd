[CmdletBinding()]
param(
    [string]$ToolchainBin = "$env:USERPROFILE/.codex/toolchains/ashford-winlibs/mingw32/bin",
    [switch]$PlatformTestsOnly,
    [switch]$ConfigureOnly,
    [switch]$Package,
    [string]$BuildDirectory,
    [switch]$FetchHeaders,
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$campaignRoot = Split-Path $PSScriptRoot -Parent
$campaignBuild = Join-Path $campaignRoot $(if ($PlatformTestsOnly) { 'build/campaign-platform' } else { 'build/campaign-windows' })
if ($BuildDirectory) {
    $campaignBuild = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $campaignRoot $BuildDirectory }
}
$campaignDecomp = Join-Path $campaignRoot 'upstream/recvx-campaign-decomp'
$campaignCC = Join-Path $ToolchainBin 'gcc.exe'
$campaignCXX = Join-Path $ToolchainBin 'g++.exe'
if ($Package -and $PlatformTestsOnly) { throw 'Platform component tests cannot produce a campaign package.' }
if (!(Test-Path -LiteralPath $campaignCC) -or !(Test-Path -LiteralPath $campaignCXX)) {
    throw "Install/extract a local i686 POSIX GCC toolchain and pass -ToolchainBin <bin-folder>. Expected: $campaignCC. See docs/campaign-backend.md."
}
$campaignMachine = & $campaignCC -dumpmachine
if ($LASTEXITCODE -or $campaignMachine -notmatch '^i686-.*mingw32$') { throw "Campaign requires i686 GCC; found: $campaignMachine" }
if ($ToolchainBin -match ' ') { throw 'This GCC distribution needs its toolchain extracted to a path without spaces. The source/build paths may contain spaces.' }
$campaignPin = & git -C $campaignDecomp rev-parse HEAD
if ($LASTEXITCODE -or $campaignPin -ne '4aac5133901da3821fe443b79d6cdd1e3b29e6da') {
    throw 'Initialize only the pinned campaign source: git submodule update --init upstream/recvx-campaign-decomp. Do not recursively fetch SDK submodules.'
}
if ($FetchHeaders) {
    & git -C $campaignDecomp submodule update --init include/recvx-decomp-katana include/recvx-decomp-cri
    if ($LASTEXITCODE) { throw 'Fetching the two pinned header dependencies failed.' }
}
if (!$PlatformTestsOnly) {
    $campaignMissing = @(
        'include/recvx-decomp-katana/KATANA/Include/ninja.h',
        'include/recvx-decomp-cri/cri/mwlib/include/cri_adxt.h',
        'include/recvx-decomp-cri/cri/mwlib/include/cri_adxf.h'
    ) | ForEach-Object { Join-Path $campaignDecomp $_ } | Where-Object { !(Test-Path -LiteralPath $_) }
    if ($campaignMissing) { throw "Campaign build blocked by missing local SDK headers:`n$($campaignMissing -join "`n")`nNo proprietary SDK is downloaded. Supply existing headers; -PlatformTestsOnly verifies independent components without building the game." }
}
$campaignOldPath = $env:PATH
try {
    $env:PATH = "$ToolchainBin;$campaignRoot/.venv/Scripts;$env:PATH"
    $campaignCMake = (Get-Command cmake -ErrorAction Stop).Source
    $campaignCTest = (Get-Command ctest -ErrorAction Stop).Source
    & $campaignCMake -S $campaignRoot -B $campaignBuild -G Ninja -DASHFORD_CAMPAIGN=ON `
        "-DASHFORD_PLATFORM_TESTS_ONLY=$(if ($PlatformTestsOnly) { 'ON' } else { 'OFF' })" `
        "-DCMAKE_C_COMPILER:FILEPATH=$campaignCC" "-DCMAKE_CXX_COMPILER:FILEPATH=$campaignCXX" `
        "-DCMAKE_RC_COMPILER:FILEPATH=$ToolchainBin/windres.exe" `
        '-DCMAKE_C_FLAGS_RELWITHDEBINFO:STRING=-O2 -g -DNDEBUG' `
        '-DCMAKE_CXX_FLAGS_RELWITHDEBINFO:STRING=-O2 -g -DNDEBUG' `
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
    if ($LASTEXITCODE) { throw 'Campaign configuration failed.' }
    if ($ConfigureOnly) { return }
    & $campaignCMake --build $campaignBuild --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Campaign build failed.' }
    & $campaignCTest --test-dir $campaignBuild --output-on-failure
    if ($LASTEXITCODE) { throw 'Campaign checks failed; packaging stopped.' }
    if ($Package) {
        $campaignDist = Join-Path $campaignBuild 'dist'
        & $campaignCMake --install $campaignBuild --prefix $campaignDist
        if ($LASTEXITCODE) { throw 'Campaign installation failed.' }
        # A reused staging folder may contain notices from the former network build.
        foreach ($obsolete in @('miniupnpc-LICENSE.txt','libnatpmp-LICENSE.txt','Monocypher-LICENSE.txt','Monocypher-LICENSE.md')) {
            $obsoletePath = Join-Path $campaignDist "licenses/$obsolete"
            if (Test-Path -LiteralPath $obsoletePath) { Remove-Item -LiteralPath $obsoletePath }
        }
        # Explicit allow-list: no ISO, imported assets, saves, compiler or SDK headers.
        Compress-Archive -LiteralPath @(
            (Join-Path $campaignDist 'AshfordsHalberd.exe'),
            (Join-Path $campaignDist 'README.md'),
            (Join-Path $campaignDist 'CHANGELOG.md'),
            (Join-Path $campaignDist 'assets'),
            (Join-Path $campaignDist 'licenses')
        ) -DestinationPath (Join-Path $campaignBuild 'AshfordsHalberd-Campaign-Windows-x86.zip') -Force
    }
} finally { $env:PATH = $campaignOldPath }
