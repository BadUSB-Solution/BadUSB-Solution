# ============================================================================
#  AbangAll Installer 빌드 스크립트
# ----------------------------------------------------------------------------
#  1. Capstone (Release|x64) 를 MSBuild 로 빌드
#  2. ISCC.exe 로 setup.iss 컴파일
#  3. 산출물: Installer\output\AbangAll-Installer.exe
#
#  Usage:
#    .\build_setup.ps1
# ============================================================================

$ErrorActionPreference = 'Stop'

# ─── 경로 ─────────────────────────────────────────────────────────────────
$repoRoot     = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$capstoneProj = Join-Path $repoRoot 'Capstone\Capstone.vcxproj'
$issFile      = Join-Path $repoRoot 'Installer\setup.iss'
$outputDir    = Join-Path $repoRoot 'Installer\output'
$exePath      = Join-Path $repoRoot 'Capstone\x64\Release\Capstone.exe'
$dbPath       = Join-Path $repoRoot 'Capstone\x64\Release\matchingDB.db'

# ─── MSBuild 자동 탐색 ───────────────────────────────────────────────────
$msbuild = Get-ChildItem -Path 'C:\Program Files*\Microsoft Visual Studio\*\*\MSBuild\Current\Bin\MSBuild.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $msbuild) {
    Write-Error 'MSBuild not found. Install Visual Studio 2022 Build Tools or later.'
    exit 1
}

# ─── ISCC.exe 자동 탐색 ──────────────────────────────────────────────────
$iscc = Get-ChildItem -Path 'C:\Program Files*\Inno Setup*\ISCC.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $iscc) {
    Write-Error 'Inno Setup not found. Download: https://jrsoftware.org/isdl.php'
    exit 1
}

Write-Output "MSBuild: $($msbuild.FullName)"
Write-Output "ISCC:    $($iscc.FullName)"
Write-Output ""

# ─── 1) Capstone 빌드 ────────────────────────────────────────────────────
Write-Output '=== [1/2] Building Capstone (Release|x64) ==='
& $msbuild.FullName $capstoneProj /p:Configuration=Release /p:Platform=x64 /v:minimal /m
if ($LASTEXITCODE -ne 0) { Write-Error 'Capstone build failed'; exit 1 }

# 산출물 확인
if (-not (Test-Path $exePath)) { Write-Error "exe not found: $exePath"; exit 1 }
if (-not (Test-Path $dbPath))  { Write-Error "matchingDB.db not found: $dbPath"; exit 1 }

Write-Output ""
Write-Output '=== [2/2] Building Installer (Inno Setup) ==='

# 출력 폴더 생성
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

# ─── 2) Inno Setup 컴파일 ────────────────────────────────────────────────
& $iscc.FullName $issFile
if ($LASTEXITCODE -ne 0) { Write-Error 'ISCC compile failed'; exit 1 }

Write-Output ""
Write-Output '=== Done ==='
$result = Join-Path $outputDir 'AbangAll-Installer.exe'
if (Test-Path $result) {
    $info = Get-Item $result
    Write-Output ("Output: {0}" -f $info.FullName)
    Write-Output ("Size:   {0:N0} bytes" -f $info.Length)
    Write-Output ("Hash:   {0}" -f (Get-FileHash -Algorithm SHA256 -Path $result).Hash)
}
