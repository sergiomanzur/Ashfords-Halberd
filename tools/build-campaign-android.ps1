[CmdletBinding()]
param(
    [string]$NdkDir,
    [switch]$PlatformTestsOnly,
    [switch]$ConfigureOnly,
    [string]$BuildDirectory = 'build/campaign-android',
    [int]$Jobs = 4,
    [string]$Serial
)
$ErrorActionPreference = 'Stop'
$campaignRoot = Split-Path $PSScriptRoot -Parent
$campaignBuild = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $campaignRoot $BuildDirectory }
$campaignDecomp = Join-Path $campaignRoot 'upstream/recvx-campaign-decomp'

# Discover the newest installed NDK (e.g. 26.2.11394342 beats 26.1.10909125).
if (!$NdkDir) {
    $ndkRoots = Get-ChildItem "$env:LOCALAPPDATA/Android/Sdk/ndk/*" -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending
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

$campaignCMake = (Get-Command cmake -ErrorAction Stop).Source
$envOldAndroidNdk = $env:ANDROID_NDK
try {
    $env:ANDROID_NDK = $NdkDir
    & $campaignCMake -S $campaignRoot -B $campaignBuild -G Ninja -DASHFORD_CAMPAIGN=ON `
        "-DASHFORD_PLATFORM_TESTS_ONLY=ON" `
        "-DCMAKE_TOOLCHAIN_FILE=campaign/cmake/android-arm64.cmake" `
        "-DANDROID_NDK=$NdkDir" `
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    if ($LASTEXITCODE) { throw 'Android campaign configuration failed.' }
    if ($ConfigureOnly) { return }
    & $campaignCMake --build $campaignBuild --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Android campaign build failed.' }
} finally { $env:ANDROID_NDK = $envOldAndroidNdk }
