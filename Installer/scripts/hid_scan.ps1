# ============================================================================
#  hid_scan.ps1 — AbangAll Installer HID Device Scanner
# ----------------------------------------------------------------------------
#  본체 Capstone 의 hid_fingerprint.cpp / hid_scan.cpp 와 동일한 로직을
#  PowerShell 로 포팅한 스크립트. 솔루션 본체가 업데이트되면 이 스크립트도
#  같이 갱신해서 일관성을 유지한다.
#
#  매핑:
#    Capstone EnumeratePresentHidDevicePaths   ←  Get-PnpDevice -Class HIDClass
#    HidFingerprint::ExtractVidPid             ←  ExtractVidPid (regex)
#    ResolveDeviceIdentityKey                  ←  ResolveIdentityKey
#    CollectCurrentNodeCandidates +
#      CollectAncestorNodeCandidates           ←  CollectNameCandidates
#    LooksLikeGenericHidLabel                  ←  IsGenericLabel
#    SelectBestCandidateName                   ←  SelectBestName
#    AddCandidateName                          ←  AddCandidate
#    BuildPathFallbackIdentity                 ←  BuildPathFallbackIdentity
#    ComputeFnv1a64                            ←  ComputeFnv1a64
#    BuildFingerprint                          ←  BuildFingerprint
#    BuildFallbackName                         ←  BuildFallbackName
#    DeviceApprovalFlow::ContainsPendingFingerprint  ←  $seen 해시 dedupe
#
#  한계 (본체와 다른 점):
#    - SetupAPI 의 GUID_DEVINTERFACE_HID 인터페이스 열거 대신 HIDClass setup
#      class 를 사용 (PowerShell 에서 SetupAPI 직접 호출이 번거로움).
#      실용적으로 동일한 결과를 반환하지만, 일부 가상 HID 는 차이가 날 수
#      있음. 솔루션 첫 실행 시 본체가 다시 정확히 계산하므로 OK.
#    - HidD_GetProductString / GetManufacturerString 생략. DEVPKEY_*
#      후보로 충분한 케이스가 대부분.
#
#  출력 한 줄 포맷:
#      Name|InstanceId|FingerprintHex16|VidHex4|PidHex4
#    UTF-8 BOM (Inno Setup LoadStringsFromFile 가 자동 인식).
# ============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'SilentlyContinue'

# ─── FNV-1a 64-bit (본체 ComputeFnv1a64 와 동일 상수) ─────────────────────
function ComputeFnv1a64 {
    param([string]$Value)
    if ($null -eq $Value) { $Value = '' }
    $hash = [uint64]1469598103934665603
    $prime = [uint64]1099511628211
    $mask = [bigint]'18446744073709551615'   # 2^64 - 1
    foreach ($ch in $Value.ToCharArray()) {
        $hash = $hash -bxor [uint64][int]$ch
        $hash = [uint64](([bigint]$hash * [bigint]$prime) -band $mask)
    }
    return $hash
}

# ─── BuildFingerprint (본체 BuildFingerprint 와 동일) ─────────────────────
#   주의: PS 자동 변수 $PID (read-only, 프로세스 ID) 와 충돌하므로 함수 매개변수
#         이름은 $ProductId / $VendorId 로. main loop 의 변수도 동일.
function BuildFingerprint {
    param([bool]$HasVidPid, [uint16]$VendorId, [uint16]$ProductId, [string]$IdentityKey)
    $identityHash = ComputeFnv1a64 -Value $IdentityKey
    if (-not $HasVidPid) { return $identityHash }
    $vidShift = [uint64]$VendorId * [uint64]281474976710656   # << 48
    $pidShift = [uint64]$ProductId * [uint64]4294967296       # << 32
    return ($vidShift -bxor $pidShift) -bxor ($identityHash -band [uint64]4294967295)
}

# ─── BuildPathFallbackIdentity ────────────────────────────────────────────
#   - 본체와 동일하게 &MI_xx, &COLxx 토큰 제거 후 lower-case.
function BuildPathFallbackIdentity {
    param([string]$DevicePath)
    $key = $DevicePath.ToLower()
    $key = $key -replace '&mi_[0-9a-f]{2}', ''
    $key = $key -replace '&col[0-9a-f]{2}', ''
    return $key
}

# ─── ResolveDeviceIdentityKey ─────────────────────────────────────────────
#   현재 노드부터 부모 chain 8 단계까지 추적:
#     1. ContainerId 발견 → 'cid:<guid>' 반환
#     2. 도중 'USB\VID_...' instance ID → 'inst:<id>' 반환
#     3. 그 외 'HID\VID_...' 와 첫 instance ID 도 fallback 저장
function ResolveIdentityKey {
    param([string]$StartInstanceId)
    $current = $StartInstanceId
    $firstInstanceId = $null
    $hidInstanceId = $null
    for ($depth = 0; $depth -lt 8; $depth++) {
        $cidProp = Get-PnpDeviceProperty -InstanceId $current -KeyName 'DEVPKEY_Device_ContainerId' -ErrorAction SilentlyContinue
        if ($cidProp -and $cidProp.Data) {
            return 'cid:' + ([string]$cidProp.Data).ToLower()
        }
        $lowered = $current.ToLower()
        if (-not $firstInstanceId) { $firstInstanceId = $lowered }
        if ($lowered -like '*usb\vid_*') { return 'inst:' + $lowered }
        if (-not $hidInstanceId -and $lowered -like '*hid\vid_*') { $hidInstanceId = $lowered }
        $parentProp = Get-PnpDeviceProperty -InstanceId $current -KeyName 'DEVPKEY_Device_Parent' -ErrorAction SilentlyContinue
        if (-not $parentProp -or -not $parentProp.Data) { break }
        $current = [string]$parentProp.Data
    }
    if ($hidInstanceId) { return 'inst:' + $hidInstanceId }
    if ($firstInstanceId) { return 'inst:' + $firstInstanceId }
    return ''
}

# ─── LooksLikeGenericHidLabel (본체와 동일 토큰 목록) ─────────────────────
$script:GenericTokens = @(
    'hid compliant',
    'hid-compliant',
    'hid keyboard',
    'hid mouse',
    'hid device',
    'vendor-defined',
    'consumer control',
    'system controller',
    'hid 규격',
    'hid 준수',
    '키보드 장치',
    '마우스',
    '사용자 컨트롤',
    '시스템 컨트롤러',
    '공급업체 정의'
)

function IsGenericLabel {
    param([string]$Name)
    if (-not $Name) { return $true }
    $lower = $Name.ToLower()
    foreach ($token in $script:GenericTokens) {
        if ($lower.Contains($token.ToLower())) { return $true }
    }
    return $false
}

# ─── AddCandidateName ─────────────────────────────────────────────────────
function AddCandidate {
    param($List, [string]$Name)
    if (-not $Name) { return }
    $trimmed = $Name.Trim()
    if (-not $trimmed) { return }
    $lowerNew = $trimmed.ToLower()
    foreach ($existing in $List) {
        if ($existing.ToLower() -eq $lowerNew) { return }
    }
    [void]$List.Add($trimmed)
}

# ─── CollectCurrentNodeCandidates + CollectAncestorNodeCandidates ────────
#   현재 노드 + 부모 6 단계 위까지 DEVPKEY 후보 수집.
#   본체의 SetupAPI registry property (SPDRP_FRIENDLYNAME / SPDRP_DEVICEDESC /
#   SPDRP_MFG) 는 DEVPKEY_Device_FriendlyName / DeviceDesc 와 동등하여 생략.
function CollectNameCandidates {
    param([string]$StartInstanceId)
    $list = New-Object System.Collections.ArrayList
    $keys = @(
        'DEVPKEY_Device_BusReportedDeviceDesc',
        'DEVPKEY_NAME',
        'DEVPKEY_Device_FriendlyName',
        'DEVPKEY_Device_DeviceDesc'
    )
    $current = $StartInstanceId
    for ($depth = 0; $depth -lt 6; $depth++) {
        foreach ($key in $keys) {
            $prop = Get-PnpDeviceProperty -InstanceId $current -KeyName $key -ErrorAction SilentlyContinue
            if ($prop -and $prop.Data) {
                AddCandidate -List $list -Name ([string]$prop.Data)
            }
        }
        $parentProp = Get-PnpDeviceProperty -InstanceId $current -KeyName 'DEVPKEY_Device_Parent' -ErrorAction SilentlyContinue
        if (-not $parentProp -or -not $parentProp.Data) { break }
        $current = [string]$parentProp.Data
    }
    return , $list
}

# ─── SelectBestCandidateName (generic 후순위) ────────────────────────────
function SelectBestName {
    param($Candidates)
    foreach ($name in $Candidates) {
        if (-not (IsGenericLabel -Name $name)) { return $name }
    }
    if ($Candidates.Count -gt 0) { return $Candidates[0] }
    return ''
}

# ─── BuildFallbackName ────────────────────────────────────────────────────
function BuildFallbackName {
    param([bool]$HasVidPid, [uint16]$VendorId, [uint16]$ProductId)
    if (-not $HasVidPid) { return 'Unknown HID Device' }
    return ('HID Device (VID_{0:X4}&PID_{1:X4})' -f $VendorId, $ProductId)
}

# ============================================================================
#  본 루프
#    Capstone:  EnumeratePresentHidDevicePaths
#             + BuildUsbItemFromPath (per path)
#             + ContainsPendingFingerprint dedupe
# ============================================================================
$hidDevices = Get-PnpDevice -PresentOnly -Class HIDClass | Where-Object { $_.Status -eq 'OK' }

$seen = @{}
$lines = New-Object System.Collections.ArrayList

foreach ($dev in $hidDevices) {
    # VID/PID 추출 (본체 HidFingerprint::ExtractVidPid)
    # NOTE: $pid 는 PS 자동 변수 (read-only) 라서 그대로 쓰면 할당이 silently
    #       실패함. main loop 와 함수 매개변수 모두 $productId / $vendorId 사용.
    $vendorId = [uint16]0
    $productId = [uint16]0
    $hasVidPid = $false
    if ($dev.InstanceId -match 'VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4})') {
        $vendorId = [Convert]::ToUInt16($Matches[1], 16)
        $productId = [Convert]::ToUInt16($Matches[2], 16)
        $hasVidPid = $true
    }

    # identityKey + fingerprint
    $identityKey = ResolveIdentityKey -StartInstanceId $dev.InstanceId
    if (-not $identityKey) {
        $identityKey = BuildPathFallbackIdentity -DevicePath $dev.InstanceId
    }
    $fp = BuildFingerprint -HasVidPid $hasVidPid -VendorId $vendorId -ProductId $productId -IdentityKey $identityKey
    $fpHex = '{0:X16}' -f $fp

    # dedupe (같은 ContainerId 의 여러 HID collection 을 1 개로 묶음)
    if ($seen.ContainsKey($fpHex)) { continue }
    $seen[$fpHex] = $true

    # 이름 후보 수집 + generic 후순위 선택
    $candidates = CollectNameCandidates -StartInstanceId $dev.InstanceId
    $name = SelectBestName -Candidates $candidates
    if (-not $name) { $name = $dev.FriendlyName }
    if (-not $name) { $name = BuildFallbackName -HasVidPid $hasVidPid -VendorId $vendorId -ProductId $productId }

    $vidHex = '{0:X4}' -f $vendorId
    $pidHex = '{0:X4}' -f $productId
    [void]$lines.Add("$name|$($dev.InstanceId)|$fpHex|$vidHex|$pidHex")
}

# UTF-8 (PS 5.1 의 Out-File -Encoding UTF8 은 BOM 포함)
$lines | Out-File -FilePath $OutputPath -Encoding UTF8
