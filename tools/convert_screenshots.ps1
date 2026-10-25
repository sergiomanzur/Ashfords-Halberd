Add-Type -AssemblyName System.Drawing
$dir = "C:\Users\sergi\.gemini\antigravity\brain\5ce08b74-1e21-4e5a-af06-182070f832ef"
$files = Get-ChildItem -Path $dir -Filter "gameplay_*.bmp"
foreach ($f in $files) {
    $bmp = [System.Drawing.Bitmap]::FromFile($f.FullName)
    $pngPath = [System.IO.Path]::ChangeExtension($f.FullName, ".png")
    $bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Converted: $pngPath"
}
