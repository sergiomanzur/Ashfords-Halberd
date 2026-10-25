[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Iso,
    [ValidateSet('Title','Gameplay','FirstPerson','WeaponSwitch','Quicksave','Overlay','Cheats','Enhanced','Fmv')][string]$Mode = 'Gameplay',
    [switch]$ThirdPerson,
    [switch]$WithLauncher,
    [switch]$QualitySettings,
    [switch]$ResumeQuicksave,
    [switch]$CrossRoomQuicksave,
    [switch]$KeyboardFirstPerson,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$campaignRoot = Split-Path $PSScriptRoot -Parent
if (!$OutputDirectory) { $OutputDirectory = Join-Path $campaignRoot "local/campaign-smoke/$Mode" }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$campaignExe = (Resolve-Path -LiteralPath $Executable).Path
$campaignIso = (Resolve-Path -LiteralPath $Iso).Path
$campaignFrames = Join-Path $OutputDirectory 'frames'
$campaignSave = Join-Path $OutputDirectory 'save'
New-Item -ItemType Directory -Force $campaignFrames,$campaignSave | Out-Null
# Isolated verification settings; never touch a player's existing configuration.
Set-Content -LiteralPath (Join-Path $campaignSave 'recvx.ini') -Encoding utf8 -Value "show_fps = 0`ndifficulty_select = 0`n"
if ($Mode -eq 'Fmv') { Add-Content -LiteralPath (Join-Path $campaignSave 'recvx.ini') -Value 'skip_intro = 0' }
if ($Mode -in @('WeaponSwitch','Quicksave','FirstPerson')) {
    # Gamepad-only permits background automation without stealing the user's focus.
    Add-Content -LiteralPath (Join-Path $campaignSave 'recvx.ini') -Value 'input_source = 2'
}
$campaignLast = if ($Mode -in @('Gameplay','FirstPerson','WeaponSwitch','Quicksave')) { 2400 } elseif ($Mode -eq 'Fmv') { 1700 } elseif ($Mode -eq 'Enhanced') { 600 } else { 360 }
if ($ResumeQuicksave) { $campaignLast = 2800 }
if ($Mode -eq 'FirstPerson') { $campaignLast = 2600 }
$campaignStart = [Diagnostics.ProcessStartInfo]::new()
$campaignStart.FileName = $campaignExe
$campaignStart.WorkingDirectory = $campaignRoot
$campaignStart.UseShellExecute = $false
$campaignStart.CreateNoWindow = $true
$campaignStart.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
$campaignStart.RedirectStandardOutput = $true
$campaignStart.RedirectStandardError = $true
foreach ($argument in @($campaignIso,'--save-dir',$campaignSave,'--size','1120x800','--windowed')) {
    $campaignStart.ArgumentList.Add($argument)
}
if ($Mode -ne 'Fmv') { $campaignStart.ArgumentList.Add('--skip-intro') }
if ($WithLauncher) {
    $campaignStart.Environment['ASHFORD_LAUNCHER_CAPTURE'] = (Join-Path $OutputDirectory 'launcher.bmp')
    $campaignStart.Environment['ASHFORD_LAUNCHER_TEST_ACTION'] = 'play'
} else { $campaignStart.ArgumentList.Add('--skip-launcher') }
if ($ThirdPerson) { $campaignStart.ArgumentList.Add('--third-person') }
$campaignStart.Environment['RECVX_EXIT_FRAME'] = "$campaignLast"
$campaignStart.Environment['RECVX_TRACE_TASKS'] = '1'
$campaignStart.Environment['RECVX_DUMP_FRAMES'] = $campaignFrames
$campaignStart.Environment['RECVX_DUMP_EVERY'] = '1'
$campaignStart.Environment['RECVX_DUMP_FROM'] = "$($campaignLast - 20)"
$campaignStart.Environment['RECVX_DUMP_OVERLAY'] = '1'
if ($QualitySettings) {
    $campaignStart.Environment['RECVX_SETTINGS'] = if ($Mode -eq 'Gameplay') {
        '1750:Experience preset=0;1850:Experience preset=1;1900:Saturation=0;2000:Saturation=125;2100:Anti-aliasing=1'
    } else { '100:Experience preset=0;160:Experience preset=1;220:Saturation=0;240:Anti-aliasing=1;260:Saturation=125' }
}
if ($Mode -eq 'Overlay') { $campaignStart.Environment['RECVX_SHOW_OVERLAY'] = '1' }
if ($Mode -eq 'Cheats') { $campaignStart.Environment['RECVX_SHOW_OVERLAY'] = '2' }
if ($Mode -eq 'Enhanced') {
    # Real pointer events: title Options, Enhanced Options, then six page advances to Cheats.
    $campaignStart.Environment['RECVX_MENU_MOUSE'] = '140:560:640:l,260:850:600:l,350:1000:267:l,370:1000:267:l,390:1000:267:l,410:1000:267:l,430:1000:267:l,450:1000:267:l'
    if ($QualitySettings) { $campaignStart.Environment['RECVX_MENU_MOUSE'] = '140:560:640:l,260:850:600:l' }
}
if ($Mode -in @('Gameplay','FirstPerson','WeaponSwitch','Quicksave','Fmv')) {
    # Real DS2 button presses through the engine's test-input adapter. No warps,
    # injected equipment, invulnerability, save state or edited game progress.
    $campaignStart.Environment['RECVX_INPUT'] = '100:down,103:-,110:cross,113:-,160:cross,163:-,200:cross,203:-,240:cross,243:-,280:cross,283:-,320:start,323:-,360:circle,363:-,400:up,520:up+right,560:up,600:start,603:-,640:circle,643:-,700:up,800:up+circle,960:left,1000:start,1003:-,1040:circle,1043:-,1100:up,1250:right,1400:up,1600:cross,1603:-,1650:start,1653:-,1700:cross,1703:-,1750:cross,1753:-,1800:circle,1803:-,1850:circle,1853:-,1900:down,2000:left,2100:down,2200:up,2300:-'
    if ($Mode -eq 'Fmv') {
        # Exercise the real decoder and real key events, without automatic movie skipping.
        $campaignStart.Environment.Remove('RECVX_SKIP_MOVIES') | Out-Null
        $campaignStart.Environment.Remove('RECVX_INPUT') | Out-Null
        $campaignStart.Environment['RECVX_MOVIE_TRACE'] = (Join-Path $OutputDirectory 'movie.log')
        $campaignStart.Environment['RECVX_KEYS'] = '1000:F8,1001:-,1200:F8,1201:-,1400:F8,1401:-,1600:F8,1601:-'
    } else { $campaignStart.Environment['RECVX_SKIP_MOVIES'] = '1' }
    if ($Mode -eq 'WeaponSwitch') {
        # Test fixture only: exercise native model changes with three carried weapons.
        $campaignStart.Environment['RECVX_EQUIP'] = '500:8:0,9:7,32:15,1:6'
        $campaignStart.Environment['RECVX_VPAD'] = '1850:leftstick,1900:leftstick+dpright,1904:leftstick,1950:leftstick+dpdown,1954:leftstick,2000:leftstick+dpleft,2004:leftstick,2050:leftstick+dpup,2054:leftstick,2100:leftstick+rightshoulder,2104:leftstick,2200:leftstick+leftshoulder,2204:-'
        $campaignStart.Environment['RECVX_WEAPON_TRACE'] = '1'
    }
    if ($Mode -eq 'FirstPerson') {
        $campaignStart.Environment['RECVX_INPUT'] = ($campaignStart.Environment['RECVX_INPUT'] -split ',1900:')[0] + ',1900:-'
        $campaignStart.Environment['RECVX_SCRIPT_PRESENT'] = '1'
        $campaignStart.Environment['RECVX_EQUIP'] = '500:9:7'
        $campaignStart.Environment['RECVX_VPAD'] = '1950:leftstick+rightstick,1954:-,1960:rightx=0.35,1980:-,2000:lefty=-0.5+rightx=0.2,2020:-,2040:lefttrigger=1,2065:lefttrigger=1+righttrigger=1,2070:lefttrigger=1,2100:-,2160:leftstick+rightstick,2164:-,2180:leftstick+rightstick,2184:-,2200:north,2204:-,2320:north,2324:-,2460:leftstick+back,2464:-,2500:leftstick+start,2504:-'
        $campaignStart.Environment['RECVX_TPS_TRACE'] = '1'
        if ($KeyboardFirstPerson) {
            $campaignStart.Environment['RECVX_VPAD'] = $campaignStart.Environment['RECVX_VPAD'] -replace '\d+:leftstick\+rightstick,', ''
            $campaignStart.Environment['RECVX_KEYS'] = '1950:F12,1951:-,2160:F12,2161:-,2180:F12,2181:-'
        }
    }
    if ($Mode -eq 'Quicksave') {
        # Isolated fixture changes health/equipment after saving. Loading must
        # restore both, room-local state and control without room initialization.
        # A second save verifies that normal player control resumes after load.
        $campaignStart.Environment['RECVX_VPAD'] = '400:leftstick+back,404:-,1900:leftstick+back,1904:-,2250:leftstick+start,2254:-,2370:leftstick+back,2374:-'
        $campaignStart.Environment['RECVX_SCRIPT_PRESENT'] = '1'
        $campaignStart.Environment['RECVX_HP'] = '2000:50'
        $campaignStart.Environment['RECVX_EQUIP'] = '1850:9:7;2000:8:0'
        if ($CrossRoomQuicksave) { $campaignStart.Environment['RECVX_TELEPORT'] = '2050:0:1:0' }
        if ($ResumeQuicksave) {
            if (!(Test-Path (Join-Path $campaignSave 'quicksave.ahs'))) { throw 'Resume test requires an existing full-room quicksave from this executable.' }
            $campaignStart.Environment['RECVX_VPAD'] = '1900:leftstick+start,1904:-,2700:leftstick+back,2704:-'
            $campaignStart.Environment.Remove('RECVX_HP') | Out-Null
            $campaignStart.Environment.Remove('RECVX_EQUIP') | Out-Null
        }
    }
    $campaignStart.Environment['RECVX_ITEM_TRACE'] = '1'
    if ($ThirdPerson) { $campaignStart.Environment['RECVX_TPS_TRACE'] = '1' }
    else { $campaignStart.Environment['RECVX_MODERN_TRACE'] = '1' }
}
$campaignProcess = [Diagnostics.Process]::Start($campaignStart)
$campaignStdout = $campaignProcess.StandardOutput.ReadToEndAsync()
$campaignStderr = $campaignProcess.StandardError.ReadToEndAsync()
$campaignWatch = [Diagnostics.Stopwatch]::StartNew()
$campaignTimeout = $false
while (!$campaignProcess.WaitForExit(1000)) {
    if ($campaignWatch.Elapsed.TotalSeconds -gt 180) {
        $campaignProcess.Kill($true)
        $campaignProcess.WaitForExit()
        $campaignTimeout = $true
        break
    }
}
$campaignLog = $campaignStdout.GetAwaiter().GetResult() + $campaignStderr.GetAwaiter().GetResult()
Set-Content -LiteralPath (Join-Path $OutputDirectory 'runtime.log') -Value $campaignLog -Encoding utf8
if ($campaignTimeout) { throw "Campaign smoke timed out; logs: $OutputDirectory/runtime.log" }
if ($campaignProcess.ExitCode -ne 0) { throw "Campaign smoke failed: exit $($campaignProcess.ExitCode). See $OutputDirectory/runtime.log" }
$campaignStop = [regex]::Match($campaignLog, 'test: exit at frame (\d+)')
if (!$campaignStop.Success -or [long]$campaignStop.Groups[1].Value -lt $campaignLast) { throw 'The campaign did not reach its verification frame.' }
if ($Mode -eq 'Gameplay' -and $campaignLog -notmatch 'tasks: [^\r\n]*\bGame\b') { throw 'The scripted New Game did not reach the original Game task.' }
if ($Mode -eq 'WeaponSwitch') {
    $weaponChanges = @([regex]::Matches($campaignLog, 'weapon switch: slot (\d+) -> (\d+), item (\d+)'))
    # Opening the native inventory sorts the two-cell launcher to the first word.
    $weaponExpected = @('2:3:8','3:4:9','4:5:32','5:2:1','2:3:8','3:2:1')
    if ($weaponChanges.Count -ne $weaponExpected.Count) { throw 'Expected four direct selections and both cycling directions; inspect runtime.log.' }
    for ($change = 0; $change -lt $weaponExpected.Count; $change++) {
        $weaponActual = ($weaponChanges[$change].Groups[1].Value,$weaponChanges[$change].Groups[2].Value,$weaponChanges[$change].Groups[3].Value) -join ':'
        if ($weaponActual -ne $weaponExpected[$change]) { throw "Unexpected weapon sequence: $weaponActual; inspect runtime.log." }
    }
    $weaponInventory = @([regex]::Matches($campaignLog, 'inventory:[^\r\n]*'))[-1].Value
    if ($weaponInventory -notmatch 'Combat Knife\(8\) 0' -or $weaponInventory -notmatch 'Handgun\(9\) 7' -or $weaponInventory -notmatch 'Magnum\(32\) 15' -or $weaponInventory -notmatch '\(1\) 6') { throw 'Carried ammunition changed during switching; inspect runtime.log.' }
}
if ($Mode -eq 'Quicksave') {
    $qsPattern = 'room (\d+/\d+) case (\d+) position ([-\d.]+ [-\d.]+ [-\d.]+) hp (\d+) inventory ([0-9a-f]+) saves (\d+) retries (\d+)'
    $qsSaved = [regex]::Match($campaignLog, "quicksave: saved $qsPattern")
    $qsRestored = [regex]::Match($campaignLog, "quicksave: restored $qsPattern")
    if (!$qsSaved.Success -or !$qsRestored.Success) { throw 'Full room quicksave/load did not complete; inspect runtime.log.' }
    if (!$ResumeQuicksave) {
        for ($part = 1; $part -le 7; $part++) {
            if ($qsSaved.Groups[$part].Value -ne $qsRestored.Groups[$part].Value) { throw 'Restored room snapshot state differed from saved state.' }
        }
        if ($campaignLog -notmatch 'quicksave: request 1 blocked' -or $campaignLog -notmatch 'current hp 50 inventory 00080000') { throw 'Unsafe-input block or mutated-state fixture was not exercised.' }
    }
    if (!(Test-Path (Join-Path $campaignSave 'quicksave.ahs'))) { throw 'Separate quickslot was not persisted.' }
    $qsExpectedSaves = if ($ResumeQuicksave) { 1 } else { 2 }
    if ([regex]::Matches($campaignLog, 'quicksave: saved room').Count -ne $qsExpectedSaves) { throw 'Normal player control did not resume after quickload.' }
    if ($CrossRoomQuicksave -and $campaignLog -notmatch 'read file = RM_0010.RDX') { throw 'Cross-room test did not load its second room.' }
    if ([regex]::Matches($campaignLog, 'read file = RM_0000.RDX').Count -ne 1) { throw 'Quickload unexpectedly reopened the saved room.' }
}
if ($Mode -eq 'FirstPerson') {
    if ([regex]::Matches($campaignLog,'camera: mode 2 frame').Count -ne 2 -or $campaignLog -notmatch 'camera: mode 0 frame') { throw 'Controller first-person toggle did not switch off and on.' }
    if ($campaignLog -notmatch 'fps: active frame 25\d+[^\r\n]*hidden 1' -or $campaignLog -notmatch 'fps: suspended frame 22\d+') { throw 'First-person view did not recover after the inventory interaction.' }
    $fpsEyePositions = @([regex]::Matches($campaignLog,'fps: active frame \d+ eye ([-\d.]+ [-\d.]+ [-\d.]+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    if ($fpsEyePositions.Count -lt 3 -or $campaignLog -notmatch 'tps: weapon mode') { throw 'First-person movement or aiming was not exercised.' }
    if ($campaignLog -notmatch 'Handgun\(9\) 6' -or $campaignLog -notmatch 'quicksave: restored room') { throw 'First-person firing or snapshot restoration was not exercised.' }
}
if ($Mode -eq 'Fmv' -and ($campaignLog -notmatch 'movie: FMV skip key accepted' -or !(Test-Path (Join-Path $OutputDirectory 'movie.log')) -or (Get-Content (Join-Path $OutputDirectory 'movie.log') -Raw) -notmatch 'frames decoded')) { throw 'The real FMV skip key and decoder cleanup were not verified; inspect runtime.log and movie.log.' }
if ($Mode -eq 'Gameplay') {
    $campaignControlTrace = if ($ThirdPerson) { 'tps: cam ' } else { 'modern: frame ' }
    if ($campaignLog -notmatch $campaignControlTrace) { throw 'New Game started, but normal player control was not verified; inspect runtime.log and screenshots.' }
    $campaignPositionPattern = if ($ThirdPerson) { 'tps: cam [^\r\n]*\| p ([-\d.]+ [-\d.]+ [-\d.]+) root' } else { 'modern: frame [^\r\n]* at \(([-\d.]+ [-\d.]+)\)' }
    $campaignPositions = @([regex]::Matches($campaignLog,$campaignPositionPattern) | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    if ($campaignPositions.Count -lt 2) { throw 'The original player did not move between distinct world positions in the control trace.' }
}
if (!(Get-ChildItem -LiteralPath $campaignFrames -Filter 'win_*.bmp')) { throw 'No window verification screenshot was captured.' }
if ($QualitySettings) {
    foreach ($expected in @('Experience preset=0 preset=0','Experience preset=1 preset=1','Saturation=0 preset=2','Saturation=125 preset=2','Anti-aliasing=1 preset=2')) {
        if (!$campaignLog.Contains($expected)) { throw "Missing shared settings transition $expected; inspect runtime.log." }
    }
    $saved = Get-Content -LiteralPath (Join-Path $campaignSave 'recvx.ini') -Raw
    if ($saved -notmatch '(?m)^saturation = 125\r?$' -or $saved -notmatch '(?m)^antialias = 1\r?$') { throw 'Picture settings were not persisted.' }
}
Write-Output "$Mode smoke passed: $campaignLast frames. Evidence: $OutputDirectory"
