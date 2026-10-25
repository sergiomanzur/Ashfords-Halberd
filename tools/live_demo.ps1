Add-Type -AssemblyName System.Drawing, System.Windows.Forms

$code = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public class WindowHelper {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

    public const int SW_RESTORE = 9;
    public const uint KEYEVENTF_KEYUP = 0x0002;

    public static void PressKey(byte vk) {
        keybd_event(vk, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(50);
        keybd_event(vk, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
        System.Threading.Thread.Sleep(100);
    }

    public static void HoldKey(byte vk, int durationMs) {
        keybd_event(vk, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(durationMs);
        keybd_event(vk, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
        System.Threading.Thread.Sleep(100);
    }

    public static Bitmap CaptureWindowArea(IntPtr hWnd) {
        RECT rc;
        GetWindowRect(hWnd, out rc);
        int width = rc.Right - rc.Left;
        int height = rc.Bottom - rc.Top;
        if (width <= 0 || height <= 0) return null;
        Bitmap bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb);
        using (Graphics g = Graphics.FromImage(bmp)) {
            g.CopyFromScreen(rc.Left, rc.Top, 0, 0, new Size(width, height), CopyPixelOperation.SourceCopy);
        }
        return bmp;
    }
}
"@

Add-Type -TypeDefinition $code -ReferencedAssemblies System.Drawing

$exePath = "C:\Users\sergi\OneDrive\Documentos\ChatGPT\Ashford's Halberd\build\windows\dist\AshfordsHalberd.exe"
$outDir = "C:\Users\sergi\.gemini\antigravity\brain\5ce08b74-1e21-4e5a-af06-182070f832ef"
if (!(Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

Write-Host "Launching Ashford's Halberd ($exePath)..."
$proc = Start-Process -FilePath $exePath -PassThru

# Wait for window
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 200
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne [IntPtr]::Zero) {
        $hwnd = $proc.MainWindowHandle
        break
    }
}

if ($hwnd -eq [IntPtr]::Zero) {
    Write-Error "Failed to obtain window handle."
    if (!$proc.HasExited) { $proc.Kill() }
    exit 1
}

Write-Host "Window detected (HWND: $hwnd). Bringing to foreground..."
[WindowHelper]::ShowWindow($hwnd, [WindowHelper]::SW_RESTORE)
[WindowHelper]::SetForegroundWindow($hwnd)
Start-Sleep -Seconds 1

# Capture 1: Main Menu
Write-Host "Capturing Main Menu..."
$menuBmp = [WindowHelper]::CaptureWindowArea($hwnd)
if ($menuBmp) {
    $menuPath = Join-Path $outDir "live_demo_1_main_menu.png"
    $menuBmp.Save($menuPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $menuBmp.Dispose()
    Write-Host "Saved: $menuPath"
}

# Select "New game" or "Corridor preview"
# By default, slot 0 is "New game". Press Enter to enter gameplay!
Write-Host "Pressing ENTER to start gameplay..."
[WindowHelper]::SetForegroundWindow($hwnd)
[WindowHelper]::PressKey(0x0D) # VK_RETURN

Start-Sleep -Seconds 2

# Capture 2: In-Game (Corridor)
Write-Host "Capturing Gameplay (Corridor)..."
$gameBmp = [WindowHelper]::CaptureWindowArea($hwnd)
if ($gameBmp) {
    $gamePath = Join-Path $outDir "live_demo_2_gameplay.png"
    $gameBmp.Save($gamePath, [System.Drawing.Imaging.ImageFormat]::Png)
    $gameBmp.Dispose()
    Write-Host "Saved: $gamePath"
}

# Simulate movement: hold W for 1 second, then tap SPACE to aim
Write-Host "Moving Claire forward and aiming weapon..."
[WindowHelper]::SetForegroundWindow($hwnd)
[WindowHelper]::HoldKey(0x57, 800) # 'W' key for 800ms
Start-Sleep -Milliseconds 200

# Open Inventory: Press TAB (VK_TAB = 0x09) or I (0x49)
Write-Host "Opening Inventory UI (TAB)..."
[WindowHelper]::PressKey(0x09) # VK_TAB
Start-Sleep -Seconds 1

# Capture 3: Inventory UI with Vital ECG Monitor
Write-Host "Capturing Inventory UI..."
$invBmp = [WindowHelper]::CaptureWindowArea($hwnd)
if ($invBmp) {
    $invPath = Join-Path $outDir "live_demo_3_inventory.png"
    $invBmp.Save($invPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $invBmp.Dispose()
    Write-Host "Saved: $invPath"
}

# Close inventory
Write-Host "Closing inventory and returning to gameplay..."
[WindowHelper]::PressKey(0x09) # VK_TAB
Start-Sleep -Milliseconds 500

# Close game
Write-Host "Exiting Ashford's Halberd..."
[WindowHelper]::PressKey(0x1B) # VK_ESCAPE to return to menu
Start-Sleep -Milliseconds 400
$proc.CloseMainWindow()
Start-Sleep -Seconds 1
if (!$proc.HasExited) {
    $proc.Kill()
}

Write-Host "Live demo completed successfully!"
