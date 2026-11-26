# GPL-3.0-or-later. Gekko2 Wii U: Windows 10/11 build script (PowerShell 5.1 or 7).
#
# Builds the Wii U apps through the devkitPro MSYS2 bash: wiiu/tools/build_all.sh (shaders, probe, normal app,
# JIT app) and then the .wuhb packages. Does not modify any project file.
#
# Usage (from any folder):
#   powershell -ExecutionPolicy Bypass -File C:\Dev\Gekko2\wiiu\tools\build_wiiu.ps1
#   ... -Clean                 # make clean for both apps first
#   ... -Jobs 4                # parallel jobs (default: number of logical CPUs)
#   ... -Repo D:\src\Gekko2    # other checkout
#   ... -Bash C:\devkitPro\msys2\usr\bin\bash.exe
# The full output goes to wiiu\out\build-<date>.log. Exit code 0 = success.

[CmdletBinding()]
param(
    [string]$Repo = "C:\Dev\Gekko2",
    [string]$Bash = "C:\devkitPro\msys2\usr\bin\bash.exe",
    [string]$DevkitPro = "C:\devkitPro",
    [int]$Jobs = 0,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
function Say($text, $color = "Gray") { Write-Host $text -ForegroundColor $color }
function Fail($text) { Say "FAILED: $text" "Red"; exit 1 }

# 1. Tools and folders
Say "Gekko2 Wii U build" "Cyan"
if (-not (Test-Path $Repo)) { Fail "repository not found: $Repo" }
if (-not (Test-Path (Join-Path $Repo "wiiu\Makefile"))) { Fail "no wiiu\Makefile in $Repo (wrong folder or branch?)" }
if (-not (Test-Path $Bash)) { Fail "devkitPro MSYS2 bash not found: $Bash (install devkitPro, see https://devkitpro.org/wiki/Getting_Started)" }
if (-not (Test-Path $DevkitPro)) { Fail "devkitPro folder not found: $DevkitPro" }
$checks = @(
    @{ Name = "devkitPPC (powerpc-eabi-gcc)"; Path = "devkitPPC\bin\powerpc-eabi-gcc.exe" },
    @{ Name = "wut (Wii U SDK)";              Path = "wut\share\wut_rules" },
    @{ Name = "wuhbtool";                      Path = "tools\bin\wuhbtool.exe" },
    @{ Name = "elf2rpl";                       Path = "tools\bin\elf2rpl.exe" }
)
$missing = @()
foreach ($c in $checks) {
    if (Test-Path (Join-Path $DevkitPro $c.Path)) { Say ("  ok       " + $c.Name) "Green" }
    else { Say ("  missing  " + $c.Name + "  (" + $c.Path + ")") "Yellow"; $missing += $c.Name }
}
if ($missing.Count -gt 0) {
    Say "Install the Wii U packages in the devkitPro MSYS2 shell:  pacman -S --needed wiiu-dev" "Yellow"
    Fail "missing tools: $($missing -join ', ')"
}

# 2. Environment (as the devkitPro installer sets it; set here for this run only)
if (-not $env:DEVKITPRO) { $env:DEVKITPRO = "/opt/devkitpro" }
if (-not $env:DEVKITPPC) { $env:DEVKITPPC = "/opt/devkitpro/devkitPPC" }
Say "  DEVKITPRO=$($env:DEVKITPRO)  DEVKITPPC=$($env:DEVKITPPC)"
if ($Jobs -le 0) { $Jobs = [Environment]::ProcessorCount }
Say "  parallel jobs: $Jobs"

# 3. Log file
$outDir = Join-Path $Repo "wiiu\out"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$log = Join-Path $outDir ("build-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".log")
Say "  log: $log"

# 4. Build through bash (paths converted with cygpath inside MSYS2)
$repoUnix = & $Bash -lc "cygpath -u '$($Repo -replace '\\','/')'"
if (-not $repoUnix) { Fail "cygpath could not convert $Repo" }
$cleanCmd = ""
if ($Clean) { $cleanCmd = "make -C wiiu clean && make -C wiiu JIT=1 clean && make -C wiiu/gx2/probe clean; " }
$script = "set -o pipefail; cd '$repoUnix' && export MAKEFLAGS=-j$Jobs DEVKITPRO='$($env:DEVKITPRO)' DEVKITPPC='$($env:DEVKITPPC)' && " +
          "export PATH=`"`$DEVKITPPC/bin:`$DEVKITPRO/tools/bin:`$PATH`" && " + $cleanCmd +
          "bash wiiu/tools/build_all.sh && make -C wiiu wuhb && make -C wiiu JIT=1 wuhb"
$start = Get-Date
& $Bash -lc $script 2>&1 | Tee-Object -FilePath $log
$rc = $LASTEXITCODE
$secs = [int]((Get-Date) - $start).TotalSeconds

# 5. Errors in the log (compiler/linker), shown even when make continued
$errors = Select-String -Path $log -Pattern "error:|undefined reference|\*\*\* \[|FAILED" -SimpleMatch:$false
if ($errors) {
    Say "Errors in the log (first 20):" "Red"
    $errors | Select-Object -First 20 | ForEach-Object { Say ("  " + $_.Line) "Red" }
}

# 6. Artifacts
$artifacts = @(
    "wiiu\gekko2-wiiu.rpx", "wiiu\gekko2-wiiu.wuhb",
    "wiiu\gekko2-wiiu-jit.rpx", "wiiu\gekko2-wiiu-jit.wuhb",
    "wiiu\gx2\probe\gekko2-wiiu-gx2probe.rpx"
)
$found = 0
Say "Artifacts:" "Cyan"
foreach ($a in $artifacts) {
    $p = Join-Path $Repo $a
    if (Test-Path $p) {
        $item = Get-Item $p
        $fresh = $item.LastWriteTime -ge $start.AddSeconds(-5)
        Say ("  {0,-45} {1,10:N0} bytes  {2}" -f $a, $item.Length, $(if ($fresh) { "built now" } else { "OLD (not rebuilt)" })) $(if ($fresh) { "Green" } else { "Yellow" })
        if ($fresh) { $found++ }
    } else {
        Say ("  {0,-45} missing" -f $a) "Red"
    }
}

# 7. Result
if ($rc -eq 0 -and -not $errors -and $found -eq $artifacts.Count) {
    Say "BUILD OK in $secs s ($found artifacts). Log: $log" "Green"
    exit 0
}
Say "BUILD FAILED (exit code $rc, $found of $($artifacts.Count) artifacts rebuilt) after $secs s. Log: $log" "Red"
exit 1
