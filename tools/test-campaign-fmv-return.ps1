[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Iso,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [switch]$WatchMovie,
    [ValidateSet('Original','ThirdPerson','FirstPerson')][string]$Camera = 'FirstPerson'
)
$ErrorActionPreference = 'Stop'
$fmvRoot = Split-Path $PSScriptRoot -Parent
$fmvOutput = [IO.Path]::GetFullPath($OutputDirectory)
$fmvFrames = Join-Path $fmvOutput 'frames'
$fmvSave = Join-Path $fmvOutput 'save'
New-Item -ItemType Directory -Force $fmvFrames,$fmvSave | Out-Null
Set-Content -LiteralPath (Join-Path $fmvSave 'recvx.ini') -Encoding utf8 -Value "show_fps = 1`ndifficulty_select = 0`ninput_source = 2`nfixed_cam_controls = 1`ntps_pad_controls = 1`n"
$fmvLast = if ($WatchMovie) { 3000 } else { 1800 }
$fmvStart = [Diagnostics.ProcessStartInfo]::new()
$fmvStart.FileName = (Resolve-Path -LiteralPath $Executable).Path
$fmvStart.WorkingDirectory = $fmvRoot
$fmvStart.UseShellExecute = $false
$fmvStart.CreateNoWindow = $true
$fmvStart.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
$fmvStart.RedirectStandardOutput = $true
$fmvStart.RedirectStandardError = $true
foreach ($fmvArgument in @((Resolve-Path -LiteralPath $Iso).Path,'--save-dir',$fmvSave,'--size','800x600','--windowed','--skip-launcher','--skip-intro')) { $fmvStart.ArgumentList.Add($fmvArgument) }
if ($Camera -eq 'FirstPerson') { $fmvStart.ArgumentList.Add('--first-person') }
if ($Camera -eq 'ThirdPerson') { $fmvStart.ArgumentList.Add('--third-person') }
foreach ($fmvOverride in @('RECVX_SKIP_MOVIES','RECVX_SKIP_CINE','RECVX_MOVIE','RECVX_FLAGS')) { $fmvStart.Environment.Remove($fmvOverride) | Out-Null }
$fmvStart.Environment['RECVX_INPUT'] = '100:down,103:-,110:cross,113:-,160:cross,163:-,200:cross,203:-,240:cross,243:-,280:cross,283:-,320:start,323:-'
# Isolated fixture: enter the real graveyard event, without setting its completion flag.
$fmvStart.Environment['RECVX_TELEPORT'] = '1:0:2:0'
$fmvStart.Environment['RECVX_WARP'] = '0:2:30:133:133:16384'
$fmvStart.Environment['RECVX_SETTINGS'] = '100:Infinite health=1'
$fmvStart.Environment['RECVX_EXIT_FRAME'] = "$fmvLast"
$fmvStart.Environment['RECVX_TRACE_TASKS'] = '1'
$fmvStart.Environment['RECVX_TPS_TRACE'] = '1'
$fmvStart.Environment['RECVX_MOVIE_TRACE'] = (Join-Path $fmvOutput 'movie.log')
$fmvStart.Environment['RECVX_DUMP_FRAMES'] = $fmvFrames
$fmvStart.Environment['RECVX_DUMP_OVERLAY'] = '1'
$fmvStart.Environment['RECVX_DUMP_FROM'] = "$($fmvLast - 6)"
$fmvStart.Environment['RECVX_DUMP_EVERY'] = '1'
if (!$WatchMovie) { $fmvStart.Environment['RECVX_KEYS'] = '400:F8,401:-,800:F8,801:-,1200:F8,1201:-' }
$fmvStarted = [DateTime]::UtcNow
$fmvProcess = [Diagnostics.Process]::Start($fmvStart)
$fmvStdout = $fmvProcess.StandardOutput.ReadToEndAsync()
$fmvStderr = $fmvProcess.StandardError.ReadToEndAsync()
if (!$fmvProcess.WaitForExit(180000)) { $fmvProcess.Kill($true); $fmvProcess.WaitForExit(); throw 'FMV return regression timed out' }
$fmvLog = $fmvStdout.GetAwaiter().GetResult() + $fmvStderr.GetAwaiter().GetResult()
Set-Content -LiteralPath (Join-Path $fmvOutput 'runtime.log') -Value $fmvLog -Encoding utf8
if ($fmvProcess.ExitCode -ne 0) { throw "FMV regression exited $($fmvProcess.ExitCode)" }
$fmvExit = [regex]::Match($fmvLog,'test: exit at frame (\d+)')
if (!$fmvExit.Success -or [int]$fmvExit.Groups[1].Value -lt $fmvLast) { throw 'The game did not reach the post-FMV verification frame' }
$fmvMovieLog = Get-Content -LiteralPath (Join-Path $fmvOutput 'movie.log') -Raw
if ($fmvMovieLog -notmatch 'movie at sector 59692,' -or $fmvMovieLog -notmatch 'frames decoded') { throw 'The actual first-zombie FMV did not run and close' }
if (!$WatchMovie -and $fmvLog -notmatch 'movie: FMV skip key accepted') { throw 'The real FMV skip input was not exercised' }
if (!$WatchMovie -and $Camera -eq 'FirstPerson') {
    $fmvAfterSkip = $fmvLog.Substring($fmvLog.IndexOf('movie: FMV skip key accepted'))
    if ($fmvAfterSkip -notmatch 'fps: active frame') { throw 'First-person view did not resume after skipping the FMV' }
}
$fmvImage = Get-ChildItem -LiteralPath $fmvFrames -Filter 'win_*.bmp' | Where-Object { $_.LastWriteTimeUtc -ge $fmvStarted } | Sort-Object Name | Select-Object -Last 1
if (!$fmvImage) { $fmvImage = Get-ChildItem -LiteralPath $fmvFrames -Filter 'frame_*.bmp' | Where-Object { $_.LastWriteTimeUtc -ge $fmvStarted } | Sort-Object Name | Select-Object -Last 1 }
if (!$fmvImage) { throw 'No post-FMV window image captured' }
Add-Type -AssemblyName System.Drawing
$fmvBitmap = [Drawing.Bitmap]::new($fmvImage.FullName)
try {
    $fmvNonBlack = 0; $fmvSamples = 0; $fmvMax = 0
    # Exclude title bar, FPS counter, borders and letterboxing.
    for ($fmvY = [int]($fmvBitmap.Height * 0.2); $fmvY -lt $fmvBitmap.Height * 0.8; $fmvY += 8) {
        for ($fmvX = [int]($fmvBitmap.Width * 0.2); $fmvX -lt $fmvBitmap.Width * 0.8; $fmvX += 8) {
            $fmvPixel = $fmvBitmap.GetPixel($fmvX,$fmvY)
            $fmvLevel = [Math]::Max($fmvPixel.R,[Math]::Max($fmvPixel.G,$fmvPixel.B))
            if ($fmvLevel -gt 8) { $fmvNonBlack++ }
            $fmvMax = [Math]::Max($fmvMax,$fmvLevel); $fmvSamples++
        }
    }
} finally { $fmvBitmap.Dispose() }
$fmvPercent = 100.0 * $fmvNonBlack / $fmvSamples
"Camera=$Camera WatchMovie=$WatchMovie Post-FMV non-black=$($fmvPercent.ToString('F2'))% maximum=$fmvMax" | Tee-Object -FilePath (Join-Path $fmvOutput 'result.txt')
if ($fmvPercent -lt 1) { throw 'Gameplay resumed behind a black image after the first-zombie FMV' }
Write-Output "FMV return regression passed: $fmvOutput"
