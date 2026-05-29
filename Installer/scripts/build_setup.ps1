# ============================================================================
#  AbangAll Installer 빌드 스크립트
# ----------------------------------------------------------------------------
#  1. Qt UI (CapstoneQtApp.exe) 를 CMake 로 Release 빌드
#     - windeployqt 가 CMake 빌드 단계에서 자동 실행되어 Qt DLL/플러그인까지 동봉됨
#  2. ISCC.exe 로 setup.iss 컴파일
#  3. 산출물: Installer\output\AbangAll-Installer.exe
#
#  Usage:
#    .\build_setup.ps1
# ============================================================================

$ErrorActionPreference = 'Stop'

# ─── 경로 ─────────────────────────────────────────────────────────────────
$repoRoot     = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildDir     = Join-Path $repoRoot 'build-qt'
$issFile      = Join-Path $repoRoot 'Installer\setup.iss'
$outputDir    = Join-Path $repoRoot 'Installer\output'
$exePath      = Join-Path $buildDir 'Release\CapstoneQtApp.exe'
$dbPath       = Join-Path $buildDir 'Release\matchingDB.db'

# ─── CMake 자동 탐색 ─────────────────────────────────────────────────────
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $cmake = Get-ChildItem -Path 'C:\Program Files\CMake\bin\cmake.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $cmake) {
    Write-Error 'CMake not found. Install CMake or add it to PATH.'
    exit 1
}

# ─── ISCC.exe 자동 탐색 ──────────────────────────────────────────────────
$iscc = Get-ChildItem -Path 'C:\Program Files*\Inno Setup*\ISCC.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $iscc) {
    Write-Error 'Inno Setup not found. Download: https://jrsoftware.org/isdl.php'
    exit 1
}

$cmakePath = if ($cmake.Source) { $cmake.Source } else { $cmake.FullName }
Write-Output "CMake:   $cmakePath"
Write-Output "ISCC:    $($iscc.FullName)"
Write-Output ""

# ─── 1) Qt UI 빌드 (CMake Release) ───────────────────────────────────────
Write-Output '=== [1/2] Building Qt UI (CMake Release) ==='
# 빌드 디렉터리가 없으면 configure 부터
if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
    & cmake -S $repoRoot -B $buildDir
    if ($LASTEXITCODE -ne 0) { Write-Error 'CMake configure failed'; exit 1 }
}
& cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { Write-Error 'Qt UI build failed'; exit 1 }

# 산출물 확인
if (-not (Test-Path $exePath)) { Write-Error "exe not found: $exePath"; exit 1 }
if (-not (Test-Path $dbPath))  {
    Write-Warning "matchingDB.db not found at $dbPath — installer may fail. Run Capstone build once to seed it."
}

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
