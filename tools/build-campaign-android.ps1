[CmdletBinding()]
param(
    [string]$NdkDir,
    [switch]$PlatformTestsOnly,
    [switch]$ConfigureOnly,
    [switch]$SkipDeviceTests,
    [string]$BuildDirectory = 'build/campaign-android',
    [int]$Jobs = 4,
    [string]$Serial
)
$ErrorActionPreference = 'Stop'
$campaignRoot = Split-Path $PSScriptRoot -Parent
$campaignBuild = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $campaignRoot $BuildDirectory }
$campaignDecomp = Join-Path $campaignRoot 'upstream/recvx-campaign-decomp'

# Discover the newest installed NDK by semantic version (e.g. 26.10 beats 26.2).
if (!$NdkDir) {
    $ndkRoots = Get-ChildItem "$env:LOCALAPPDATA/Android/Sdk/ndk/*" -Directory -ErrorAction SilentlyContinue |
                Sort-Object { $v = $null; if ([version]::TryParse($_.Name, [ref]$v)) { $v } else { [version]'0.0' } }, Name -Descending
    if (!$ndkRoots) { throw "No Android NDK found under `$env:LOCALAPPDATA/Android/Sdk/ndk. Install the NDK or pass -NdkDir." }
    $NdkDir = $ndkRoots[0].FullName
}
$ndkClang = Join-Path $NdkDir 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
if (!(Test-Path -LiteralPath $ndkClang)) {
    throw "NDK clang missing: $ndkClang. Is -NdkDir pointing to a valid NDK root?"
}

# The campaign backend is pinned to a specific decomp commit.
$campaignPin = & git -C $campaignDecomp rev-parse HEAD
if ($LASTEXITCODE -or $campaignPin -ne '4aac5133901da3821fe443b79d6cdd1e3b29e6da') {
    throw 'Initialize only the pinned campaign source: git submodule update --init upstream/recvx-campaign-decomp. Do not recursively fetch SDK submodules.'
}
if (!$PlatformTestsOnly) {
    $campaignMissing = @(
        'include/recvx-decomp-katana/KATANA/Include/ninja.h',
        'include/recvx-decomp-cri/cri/mwlib/include/cri_adxt.h',
        'include/recvx-decomp-cri/cri/mwlib/include/cri_adxf.h'
    ) | ForEach-Object { Join-Path $campaignDecomp $_ } | Where-Object { !(Test-Path -LiteralPath $_) }
    if ($campaignMissing) { throw "Android campaign build blocked by missing local SDK headers:`n$($campaignMissing -join "`n")`nUse -PlatformTestsOnly for independent component checks without building librecvx_game.so." }
}

$campaignCMake = (Get-Command cmake -ErrorAction Stop).Source
$envOldAndroidNdk = $env:ANDROID_NDK
try {
    $env:ANDROID_NDK = $NdkDir
    & $campaignCMake -S $campaignRoot -B $campaignBuild -G Ninja -DASHFORD_CAMPAIGN=ON `
        "-DASHFORD_PLATFORM_TESTS_ONLY=$(if ($PlatformTestsOnly) { 'ON' } else { 'OFF' })" `
        "-DCMAKE_TOOLCHAIN_FILE=campaign/cmake/android-arm64.cmake" `
        "-DANDROID_NDK=$NdkDir" `
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    if ($LASTEXITCODE) { throw 'Android campaign configuration failed.' }
    if ($ConfigureOnly) { return }
    & $campaignCMake --build $campaignBuild --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Android campaign build failed.' }

    if ($SkipDeviceTests) { return }
    # Run the on-device test suite.
    $runner = Join-Path $PSScriptRoot 'run-android-ctest.py'
    $runnerArgs = @($runner, '--build-dir', $campaignBuild)
    if ($PlatformTestsOnly) { $runnerArgs += '--platform-tests-only' }
    if ($Serial) { $runnerArgs += @('--serial', $Serial) }
    & python @runnerArgs
    if ($LASTEXITCODE) { throw 'On-device ctest run reported failures.' }
} finally { $env:ANDROID_NDK = $envOldAndroidNdk }
