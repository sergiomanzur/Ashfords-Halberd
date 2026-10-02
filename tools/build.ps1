$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $env:PATH = "$root/.venv/Scripts;$env:PATH"
    $gcc = (Get-Command gcc -ErrorAction Stop).Source
    $gxx = (Get-Command g++ -ErrorAction Stop).Source
    if (!(Test-Path 'build/sdk/lib/cmake/SDL2/SDL2Config.cmake')) {
        ./tools/verify.ps1
        cmake --install build/toolchain-smoke --prefix build/sdk
        if ($LASTEXITCODE) { throw 'SDL installation failed' }
    }
    & ./.venv/Scripts/python.exe tests/fixtures.py local/test-fixtures
    if ($LASTEXITCODE) { throw 'Fixture generation failed' }
    cmake -S . -B build/windows -G Ninja "-DCMAKE_C_COMPILER=$gcc" "-DCMAKE_CXX_COMPILER=$gxx" "-DCMAKE_PREFIX_PATH=$root/build/sdk" "-DPython3_EXECUTABLE=$root/.venv/Scripts/python.exe" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    if ($LASTEXITCODE) { throw 'Windows configuration failed' }
    cmake --build build/windows --parallel 4
    if ($LASTEXITCODE) { throw 'Windows build failed' }
    ctest --test-dir build/windows --output-on-failure
    if ($LASTEXITCODE) { throw 'Windows checks failed' }
    cmake --install build/windows --prefix build/windows/dist
    if ($LASTEXITCODE) { throw 'Packaging failed' }
    $package = Join-Path $root 'build/windows/AshfordsHalberd-Windows-foundation.zip'
    Compress-Archive -LiteralPath @('build/windows/dist/AshfordsHalberd.exe', 'build/windows/dist/README.txt', 'build/windows/dist/licenses') -DestinationPath $package -Force
    if (!(Test-Path -LiteralPath $package)) { throw 'ZIP packaging failed' }
    Write-Host "Windows executable: $root/build/windows/dist/AshfordsHalberd.exe"
    Write-Host "Windows package: $package"
} finally { Pop-Location }
