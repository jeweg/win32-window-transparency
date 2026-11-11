# Runs the demos one after the other and saves a screenshot of each to
# screenshots/, cropped to the checkerboard behind the demo window. Console
# output, if a demo has any, is saved next to the screenshot. The system the
# screenshots were taken on goes into capture-info.txt in the same directory.
#
#   powershell -ExecutionPolicy Bypass -File capture_screenshots.ps1
#   powershell -ExecutionPolicy Bypass -File capture_screenshots.ps1 -Only 05_perpixel_alpha,12_dxgi_present_blit
#   powershell -ExecutionPolicy Bypass -File capture_screenshots.ps1 -OutDir screenshots\layered_on_dxgi -Only 14_opengl_perpixel_alpha -Note "Vulkan/OpenGL present method: Prefer layered on DXGI Swapchain"
#
# -Only takes demo names (all variants) or screenshot names (one variant).
# -Note goes into capture-info.txt, for settings the script can't read, like
# the driver's control panel. The script exits with 1 if any screenshot
# couldn't be taken.
#
# The demo windows show up on your screen for a few seconds each, don't move
# the mouse over them meanwhile.

param(
    [string]$BuildDir = '',
    [string]$OutDir = "$PSScriptRoot\screenshots",
    [string[]]$Only = @(),
    [string]$Note = ''
)

$ErrorActionPreference = 'Stop'

if (-not $BuildDir) {
    # Visual Studio generators build into build\Release, Ninja and the like into build.
    $BuildDir = Join-Path $PSScriptRoot 'build\Release'
    if (-not (Test-Path $BuildDir)) { $BuildDir = Join-Path $PSScriptRoot 'build' }
}
# powershell -File passes "a,b" as one string.
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class Win {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, StringBuilder name, int count);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern uint GetDpiForSystem();

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    public static string ClassOf(IntPtr hwnd) {
        var name = new StringBuilder(256);
        GetClassName(hwnd, name, name.Capacity);
        return name.ToString();
    }

    // Visible top-level windows of a process, in z-order (topmost first).
    public static IntPtr[] WindowsOf(uint processId) {
        var result = new List<IntPtr>();
        EnumWindows((hwnd, lParam) => {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId && IsWindowVisible(hwnd)) result.Add(hwnd);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }
}
"@
[void][Win]::SetProcessDPIAware()

# Name is the executable; Args and Suffix make variants of the same demo.
$shots = @(
    @{ Name = '01_basic_opaque' },
    @{ Name = '02_popup_no_decorations' },
    @{ Name = '03_win11_backdrop' },
    @{ Name = '04_layered_uniform_alpha' },
    @{ Name = '05_perpixel_alpha' },
    @{ Name = '06_dwm_opt_in'; Args = '--blur'; Suffix = 'blur' },
    @{ Name = '06_dwm_opt_in'; Args = '--blur-empty'; Suffix = 'blur_empty' },
    @{ Name = '06_dwm_opt_in'; Args = '--blur-null'; Suffix = 'blur_null' },
    @{ Name = '06_dwm_opt_in'; Args = '--extend-frame'; Suffix = 'extend_frame' },
    @{ Name = '06_dwm_opt_in'; Args = '--none'; Suffix = 'none' },
    @{ Name = '07_gdi_alpha' },
    @{ Name = '07_gdi_alpha'; Args = '--no-blur'; Suffix = 'no_blur' },
    @{ Name = '07_gdi_alpha'; Args = '--layered'; Suffix = 'layered' },
    @{ Name = '08_perpixel_alpha_patblt' },
    @{ Name = '09_nothing_paints' },
    @{ Name = '09_nothing_paints'; Args = '--popup'; Suffix = 'popup' },
    @{ Name = '09_nothing_paints'; Args = '--popup --layered'; Suffix = 'popup_layered' },
    @{ Name = '09_nothing_paints'; Args = '--black-brush'; Suffix = 'black_brush' },
    @{ Name = '09_nothing_paints'; Args = '--layered'; Suffix = 'layered' },
    @{ Name = '09_nothing_paints'; Args = '--black-brush --layered'; Suffix = 'black_brush_layered' },
    @{ Name = '10_glfw_transparency_bug' },
    @{ Name = '11_premultiplied_alpha' },
    @{ Name = '12_dxgi_present'; Args = '--composition'; Suffix = 'composition' },
    @{ Name = '12_dxgi_present'; Args = '--composition --null-brush'; Suffix = 'composition_null_brush' },
    @{ Name = '12_dxgi_present'; Args = '--composition --no-blur'; Suffix = 'composition_no_blur' },
    @{ Name = '12_dxgi_present'; Args = '--composition --no-redirection-bitmap'; Suffix = 'composition_no_redirection_bitmap' },
    @{ Name = '12_dxgi_present'; Args = '--composition --no-redirection-bitmap --null-brush --no-blur'; Suffix = 'composition_no_redirection_bitmap_only' },
    @{ Name = '12_dxgi_present'; Args = '--flip'; Suffix = 'flip' },
    @{ Name = '12_dxgi_present'; Args = '--flip --no-redirection-bitmap'; Suffix = 'flip_no_redirection_bitmap' },
    @{ Name = '12_dxgi_present'; Args = '--blit'; Suffix = 'blit' },
    @{ Name = '12_dxgi_present'; Args = '--blit --no-blur'; Suffix = 'blit_no_blur' },
    @{ Name = '12_dxgi_present'; Args = '--blit --no-redirection-bitmap'; Suffix = 'blit_no_redirection_bitmap' },
    @{ Name = '13_vulkan_perpixel_alpha' },
    @{ Name = '14_opengl_perpixel_alpha' },
    @{ Name = '14_opengl_perpixel_alpha'; Args = '--layered'; Suffix = 'layered' },
    @{ Name = '14_opengl_perpixel_alpha'; Args = '--layered --no-own-dc'; Suffix = 'layered_no_own_dc' },
    @{ Name = '14_opengl_perpixel_alpha'; Args = '--no-blur'; Suffix = 'no_blur' },
    @{ Name = '14_opengl_perpixel_alpha'; Args = '--layered --no-blur'; Suffix = 'layered_no_blur' }
)

$HWND_TOPMOST = [IntPtr](-1)
$SWP_NOSIZE_NOMOVE_NOACTIVATE = 0x0001 -bor 0x0002 -bor 0x0010

New-Item -ItemType Directory -Force $OutDir | Out-Null
# .NET would resolve a relative path against the process directory, not the PowerShell location.
$OutDir = (Resolve-Path $OutDir).Path
$taken = New-Object System.Collections.Generic.List[string]
$missed = New-Object System.Collections.Generic.List[string]

foreach ($shot in $shots) {
    $fileName = if ($shot.Suffix) { "$($shot.Name)_$($shot.Suffix)" } else { $shot.Name }
    if ($Only.Count -and $Only -notcontains $shot.Name -and $Only -notcontains $fileName) { continue }
    $png = Join-Path $OutDir "$fileName.png"
    $log = Join-Path $OutDir "$fileName.txt"
    # A screenshot from an earlier run must not pass for this one.
    Remove-Item $png, $log -ErrorAction SilentlyContinue

    $exe = Join-Path $BuildDir "$($shot.Name).exe"
    if (-not (Test-Path $exe)) {
        Write-Warning "$exe not built, skipping"
        $missed.Add($fileName)
        continue
    }

    $startArgs = @{ FilePath = $exe; PassThru = $true; NoNewWindow = $true; RedirectStandardOutput = $log }
    if ($shot.Args) { $startArgs.ArgumentList = $shot.Args }
    $process = Start-Process @startArgs

    try {
        $backdrop = [IntPtr]::Zero
        for ($i = 0; $i -lt 100 -and $backdrop -eq [IntPtr]::Zero; $i++) {
            Start-Sleep -Milliseconds 100
            foreach ($hwnd in [Win]::WindowsOf($process.Id)) {
                if ([Win]::ClassOf($hwnd) -eq 'CheckerboardBackdrop') { $backdrop = $hwnd }
            }
        }
        if ($backdrop -eq [IntPtr]::Zero) {
            Write-Warning "$fileName`: no checkerboard window showed up"
            $missed.Add($fileName)
            continue
        }

        # Give it time to render, then move the backdrop and everything else of
        # the demo in front of all other windows, keeping their relative order.
        Start-Sleep -Milliseconds 1500
        $windows = [Win]::WindowsOf($process.Id)
        [array]::Reverse($windows)
        foreach ($hwnd in $windows) {
            [void][Win]::SetWindowPos($hwnd, $HWND_TOPMOST, 0, 0, 0, 0, $SWP_NOSIZE_NOMOVE_NOACTIVATE)
        }
        Start-Sleep -Milliseconds 500

        $rect = New-Object Win+RECT
        [void][Win]::GetWindowRect($backdrop, [ref]$rect)
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        $bitmap = New-Object System.Drawing.Bitmap $width, $height
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        $graphics.Dispose()
        $bitmap.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        $taken.Add($fileName)
        Write-Output "$fileName.png"
    } catch {
        Write-Warning "$fileName`: $_"
        $missed.Add($fileName)
    } finally {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
        if ((Test-Path $log) -and (Get-Item $log).Length -eq 0) { Remove-Item $log }
        Start-Sleep -Milliseconds 300
    }
}

# What the screenshots were taken on.
$os = Get-CimInstance Win32_OperatingSystem
$cv = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$personalize = Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' -ErrorAction SilentlyContinue
$dpi = [Win]::GetDpiForSystem()
$info = New-Object System.Collections.Generic.List[string]
$info.Add("Captured $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
$info.Add("$($os.Caption) $($cv.DisplayVersion), build $($cv.CurrentBuild).$($cv.UBR)")
foreach ($gpu in Get-CimInstance Win32_VideoController) {
    $line = "GPU: $($gpu.Name), driver $($gpu.DriverVersion)"
    # NVIDIA's version is the last five digits of the last two fields: 32.0.16.1047 is 610.47.
    $parts = "$($gpu.DriverVersion)".Split('.')
    if ($gpu.Name -match 'NVIDIA' -and $parts.Count -eq 4) {
        $digits = $parts[2] + $parts[3]
        if ($digits.Length -ge 5) {
            $digits = $digits.Substring($digits.Length - 5)
            $line += " (NVIDIA $($digits.Substring(0, 3)).$($digits.Substring(3)))"
        }
    }
    $info.Add($line)
}
$info.Add("System DPI: $dpi ($([math]::Round($dpi * 100 / 96))%)")
if ($personalize) {
    $theme = if ($personalize.AppsUseLightTheme -eq 0) { 'dark' } else { 'light' }
    $effects = if ($personalize.EnableTransparency -eq 0) { 'off' } else { 'on' }
    $info.Add("Apps theme: $theme, transparency effects: $effects")
}
# Settings > Display > Graphics, can turn blt-model swapchains into flip model.
$gpuPrefs = Get-ItemProperty 'HKCU:\Software\Microsoft\DirectX\UserGpuPreferences' -ErrorAction SilentlyContinue
$upgrade = if ($gpuPrefs.DirectXUserGlobalSettings -match 'SwapEffectUpgradeEnable=(\d)') { if ($Matches[1] -eq '1') { 'on' } else { 'off' } } else { 'not set' }
$info.Add("Optimizations for windowed games: $upgrade")
if ($gpuPrefs) {
    foreach ($property in $gpuPrefs.PSObject.Properties) {
        if ($property.Name -like "$BuildDir\*") { $info.Add("Per-app graphics settings for $(Split-Path $property.Name -Leaf): $($property.Value)") }
    }
}
$info.Add("Screenshots: $(if ($Only.Count) { $taken -join ', ' } else { "full run, $($taken.Count) taken" })")
if ($missed.Count) { $info.Add("Not taken: $($missed -join ', ')") }
if ($Note) { $info.Add("Note: $Note") }
$infoFile = Join-Path $OutDir 'capture-info.txt'
$text = ($info -join "`n") + "`n"
if ($Only.Count -and (Test-Path $infoFile)) {
    [System.IO.File]::AppendAllText($infoFile, "`n" + $text)
} else {
    [System.IO.File]::WriteAllText($infoFile, $text)
}

if ($missed.Count) {
    Write-Warning "Not taken: $($missed -join ', ')"
    exit 1
}
