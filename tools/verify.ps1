$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $env:PATH = "$root/.venv/Scripts;$env:PATH"
    $gcc = (Get-Command gcc -ErrorAction Stop).Source
    $gxx = (Get-Command g++ -ErrorAction Stop).Source
    cmake -S tools/smoke -B build/toolchain-smoke -G Ninja "-DCMAKE_C_COMPILER=$gcc" "-DCMAKE_CXX_COMPILER=$gxx"
    if ($LASTEXITCODE) { throw 'Native CMake configuration failed' }
    cmake --build build/toolchain-smoke --parallel 4
    if ($LASTEXITCODE) { throw 'Native build failed' }
    ctest --test-dir build/toolchain-smoke --output-on-failure
    if ($LASTEXITCODE) { throw 'Native smoke check failed' }
    & ./.venv/Scripts/python.exe -c 'import pycdlib, elftools, capstone, PIL, splat; print("Python asset and analysis tools OK")'
    if ($LASTEXITCODE) { throw 'Python tool import check failed' }
} finally { Pop-Location }
