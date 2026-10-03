# Development-only independent decoder. Never used by the game runtime.
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskDestination = Join-Path $taskRoot 'local/tools/vgmstream-r2117'
$taskArchive = Join-Path $taskRoot 'local/tools/vgmstream-r2117-win64.zip'
$taskUrl = 'https://github.com/vgmstream/vgmstream/releases/download/r2117/vgmstream-win64.zip'
$taskDigest = '6c4a8a3813864fefed081bbd337dbc0ad93bf88e0b92f5db98d7ab258b22dc6c'
New-Item -ItemType Directory -Path (Split-Path $taskArchive -Parent) -Force | Out-Null
if (!(Test-Path -LiteralPath $taskArchive)) {
    Invoke-WebRequest -Uri $taskUrl -OutFile $taskArchive
}
if ((Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskDigest) {
    throw 'Audio oracle archive SHA256 mismatch; refusing extraction'
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskZip = [IO.Compression.ZipFile]::OpenRead($taskArchive)
try {
    $taskPrefix = [IO.Path]::GetFullPath($taskDestination) + [IO.Path]::DirectorySeparatorChar
    foreach ($taskEntry in $taskZip.Entries) {
        $taskTarget = [IO.Path]::GetFullPath((Join-Path $taskDestination $taskEntry.FullName))
        if (!$taskTarget.StartsWith($taskPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Audio oracle archive path escapes destination'
        }
    }
} finally { $taskZip.Dispose() }
if (!(Test-Path -LiteralPath $taskDestination)) {
    Expand-Archive -LiteralPath $taskArchive -DestinationPath $taskDestination
}
$taskExecutable = Join-Path $taskDestination 'vgmstream-cli.exe'
if (!(Test-Path -LiteralPath $taskExecutable)) { throw 'Audio oracle CLI missing' }
Write-Output "Headless PCM oracle: $taskExecutable"
