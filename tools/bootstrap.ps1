$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if (!(Test-Path '.venv/Scripts/python.exe')) {
        python -m venv .venv
        if ($LASTEXITCODE) { throw 'Unable to create Python environment' }
    }
    & ./.venv/Scripts/python.exe -m pip install -r tools/requirements.lock.txt
    if ($LASTEXITCODE) { throw 'Python dependencies failed' }
    git submodule update --init upstream/recvx-decomp external/SDL
    if ($LASTEXITCODE) { throw 'Source dependency checkout failed' }
    $commands = @{}
    foreach ($name in @('git', 'gcc', 'g++', 'gdb', 'clang', 'clang-format', 'llvm-objdump', 'llvm-readobj', '7z')) {
        $command = Get-Command $name -ErrorAction Stop
        $commands[$name] = $command.Source
    }
    $commands | ConvertTo-Json | Set-Content tools/local-toolchain.json
    Write-Host 'Dependencies ready. Run tools/verify.ps1 to build and test the native toolchain.'
} finally { Pop-Location }
