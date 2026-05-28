; ============================================================================
;  AbangAll Installer (Inno Setup)
; ----------------------------------------------------------------------------
;  Build:
;    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" setup.iss
;
;  Output:
;    Installer\output\AbangAll-Installer.exe
;
;  Requirements:
;    - Capstone (Release|x64) 빌드가 먼저 끝나 있어야 함.
;      산출물: ..\Capstone\x64\Release\Capstone.exe
;              ..\Capstone\x64\Release\matchingDB.db
; ============================================================================

#define AppName        "AbangAll"
#define AppVersion     "1.0.0"
#define AppPublisher   "AbangAll Team"
#define AppExeName     "AbangAll.exe"
; Qt UI 빌드 산출물 디렉터리 (CMake가 windeployqt까지 자동 처리해서 모든 Qt 의존성 동봉됨)
#define SourceDir      "..\build-qt\Release"
#define SourceExePath  SourceDir + "\CapstoneQtApp.exe"
#define SourceDbPath   SourceDir + "\matchingDB.db"

[Setup]
AppId={{B3A8F7C2-4A1E-4B5F-9B2A-7D1E5F8C3D4E}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=
AppSupportURL=
AppUpdatesURL=

; 설치 경로: C:\Program Files\AbangAll\
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}

; Admin 권한 필수 (블랙리스트 기능 + Program Files 쓰기)
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=

; x64 전용
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; 인스톨러 출력
OutputDir=output
OutputBaseFilename=AbangAll-Installer
Compression=lzma2/ultra
SolidCompression=yes

; 마법사 외관
WizardStyle=modern
DisableWelcomePage=no
DisableProgramGroupPage=yes
ShowLanguageDialog=yes
; 마법사 + 인스톨러 EXE 자체 아이콘
SetupIconFile=assets\icon.ico
UninstallDisplayName={#AppName}
; 언인스톨러도 같은 아이콘으로 (제어판 / 설정→앱 표시용)
UninstallDisplayIcon={app}\{#AppExeName}

; 언인스톨 정리
CloseApplications=force
RestartApplications=no

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
; ─── 한국어 ─────────────────────────────────────────────────────────────
korean.AdditionalIconsGroup=추가 아이콘:
korean.MenuIconDesc=시작 메뉴에 단축키 만들기
korean.DesktopIconDesc=바탕화면에 단축키 만들기
korean.UninstallAppDataPrompt=AbangAll 관련 파일(정책 / 설정 / 로그)도 함께 삭제하시겠습니까?
korean.UninstallAppDataKeep=아니오를 선택하면 다음 설치 시 기존 정책이 그대로 유지됩니다.
korean.HidScanCaption=기존 USB 장치 검사
korean.HidScanDescription=현재 연결된 USB/HID 장치를 검사합니다. 허용할 장치를 선택하세요. 체크하지 않은 장치는 차단됩니다.
korean.HidScanInProgress=장치 검사 중...
korean.HidScanNoDevices=검색된 HID 장치가 없습니다.

; ─── English ───────────────────────────────────────────────────────────
english.AdditionalIconsGroup=Additional icons:
english.MenuIconDesc=Create a Start Menu shortcut
english.DesktopIconDesc=Create a Desktop shortcut
english.UninstallAppDataPrompt=Also remove AbangAll-related files (policy / settings / logs)?
english.UninstallAppDataKeep=Selecting No will preserve existing policy for next install.
english.HidScanCaption=Existing USB Devices
english.HidScanDescription=Scanning currently connected USB/HID devices. Check the devices to allow. Unchecked devices will be blocked.
english.HidScanInProgress=Scanning devices...
english.HidScanNoDevices=No HID devices found.

[Tasks]
; 사용자가 체크박스로 단축키 위치 선택
Name: "menuicon"; \
    Description: "{cm:MenuIconDesc}"; \
    GroupDescription: "{cm:AdditionalIconsGroup}"
Name: "desktopicon"; \
    Description: "{cm:DesktopIconDesc}"; \
    GroupDescription: "{cm:AdditionalIconsGroup}"; \
    Flags: unchecked

[Files]
; ── 본체 — CapstoneQtApp.exe 를 AbangAll.exe 로 리네임해서 배포
Source: "{#SourceExePath}"; DestDir: "{app}"; DestName: "{#AppExeName}"; Flags: ignoreversion

; ── 시그니처 DB
Source: "{#SourceDbPath}"; DestDir: "{app}"; Flags: ignoreversion

; ── Qt6 핵심 DLL (windeployqt가 build-qt/Release에 자동 배치)
Source: "{#SourceDir}\Qt6Core.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Gui.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Widgets.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Network.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Svg.dll";        DestDir: "{app}"; Flags: ignoreversion

; ── 시스템 의존 DLL
Source: "{#SourceDir}\D3Dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\opengl32sw.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\icuuc.dll";          DestDir: "{app}"; Flags: ignoreversion

; ── Qt 플러그인 디렉터리들 (Windows 플랫폼 플러그인은 필수)
Source: "{#SourceDir}\platforms\*";          DestDir: "{app}\platforms";          Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\styles\*";             DestDir: "{app}\styles";             Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\imageformats\*";       DestDir: "{app}\imageformats";       Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\iconengines\*";        DestDir: "{app}\iconengines";        Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\tls\*";                DestDir: "{app}\tls";                Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\generic\*";            DestDir: "{app}\generic";            Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\translations\*";       DestDir: "{app}\translations";       Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; 시작 메뉴 단축키
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: menuicon
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"; Tasks: menuicon

; 바탕화면 단축키
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; 설치 완료 후 즉시 실행 (사용자가 체크박스 선택 시)
;   - 솔루션 첫 실행이 자동으로 기존 USB 스캔 + 일괄 승인 UI 띄움
Filename: "{app}\{#AppExeName}"; \
    Description: "{cm:LaunchProgram,{#AppName}}"; \
    Flags: nowait postinstall skipifsilent runascurrentuser

[UninstallDelete]
; Program Files 안의 파일은 Inno Setup 이 자동 정리. AppData 는 [Code] 에서 별도 처리.

[Code]
// ============================================================================
// 인스톨 마법사 — 기존 USB/HID 장치 스캔 + 일괄 승인 페이지
// ============================================================================

// ─── Windows API 상수 ──────────────────────────────────────────────────────
const
  // GUID_DEVINTERFACE_HID = {4D1E55B2-F16F-11CF-88CB-001111000030}
  // Inno Setup Pascal 에선 TGUID 상수 직접 선언 안 되니까 inline 으로 채움.

  DIGCF_PRESENT              = $00000002;
  DIGCF_DEVICEINTERFACE      = $00000010;
  ERROR_INSUFFICIENT_BUFFER  = 122;
  ERROR_NO_MORE_ITEMS        = 259;
  INVALID_HANDLE_VALUE_INT   = -1;
  GENERIC_READ               = $80000000;
  GENERIC_WRITE              = $40000000;
  FILE_SHARE_READ            = $00000001;
  FILE_SHARE_WRITE           = $00000002;
  OPEN_EXISTING              = 3;

// ─── Windows API 구조체 ────────────────────────────────────────────────────
type
  TSpDeviceInterfaceData = record
    cbSize:             DWORD;
    InterfaceClassGuid: TGUID;
    Flags:              DWORD;
    Reserved:           DWORD;   // 32-bit Setup: ULONG_PTR == DWORD
  end;

  // SP_DEVICE_INTERFACE_DETAIL_DATA_W 의 32-bit alignment.
  //   - 첫 DWORD = cbSize (32-bit Setup 에선 6 [DWORD + WCHAR] 의 정렬 후 6 또는 4+2=6).
  //   - 이어 DevicePath 가 가변 길이로 따라옴 → 별도 buffer 로 처리.
  THiddAttributes = record
    Size:           DWORD;
    VendorID:       Word;
    ProductID:      Word;
    VersionNumber:  Word;
  end;

// ─── setupapi.dll ──────────────────────────────────────────────────────────
function SetupDiGetClassDevsW(
    const ClassGuid: TGUID;
    Enumerator:      PAnsiChar;   // NULL = 모든 enumerator
    hwndParent:      HWND;
    Flags:           DWORD): THandle;
  external 'SetupDiGetClassDevsW@setupapi.dll stdcall';

function SetupDiEnumDeviceInterfaces(
    DeviceInfoSet:           THandle;
    DeviceInfoData:          Cardinal;   // NULL
    const InterfaceClassGuid:TGUID;
    MemberIndex:             DWORD;
    var DeviceInterfaceData: TSpDeviceInterfaceData): Boolean;
  external 'SetupDiEnumDeviceInterfaces@setupapi.dll stdcall';

function SetupDiGetDeviceInterfaceDetailW(
    DeviceInfoSet:                  THandle;
    var DeviceInterfaceData:        TSpDeviceInterfaceData;
    DeviceInterfaceDetailData:      Cardinal;   // pointer (NULL on size probe)
    DeviceInterfaceDetailDataSize:  DWORD;
    var RequiredSize:               DWORD;
    DeviceInfoData:                 Cardinal): Boolean;
  external 'SetupDiGetDeviceInterfaceDetailW@setupapi.dll stdcall';

// 실제 버퍼에 채우는 두 번째 호출용 — 버퍼 raw 포인터 받는 형태.
function SetupDiGetDeviceInterfaceDetailW_Fill(
    DeviceInfoSet:                  THandle;
    var DeviceInterfaceData:        TSpDeviceInterfaceData;
    DeviceInterfaceDetailData:      AnsiString;
    DeviceInterfaceDetailDataSize:  DWORD;
    var RequiredSize:               DWORD;
    DeviceInfoData:                 Cardinal): Boolean;
  external 'SetupDiGetDeviceInterfaceDetailW@setupapi.dll stdcall';

function SetupDiDestroyDeviceInfoList(DeviceInfoSet: THandle): Boolean;
  external 'SetupDiDestroyDeviceInfoList@setupapi.dll stdcall';

// ─── kernel32.dll ──────────────────────────────────────────────────────────
function CreateFileW(
    FileName:           string;
    DesiredAccess:      DWORD;
    ShareMode:          DWORD;
    SecurityAttributes: Cardinal;
    CreationDisposition:DWORD;
    FlagsAndAttributes: DWORD;
    TemplateFile:       THandle): THandle;
  external 'CreateFileW@kernel32.dll stdcall';

function CloseHandle(hObject: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

// ─── hid.dll ───────────────────────────────────────────────────────────────
function HidD_GetAttributes(
    HidDeviceObject: THandle;
    var Attributes:  THiddAttributes): Boolean;
  external 'HidD_GetAttributes@hid.dll stdcall';

function HidD_GetProductString(
    HidDeviceObject: THandle;
    Buffer:          string;
    BufferLength:    DWORD): Boolean;
  external 'HidD_GetProductString@hid.dll stdcall';

// ─── 데이터 타입 ──────────────────────────────────────────────────────────
type
  THidDevice = record
    Vid:         Word;       // 0x046D
    Pid:         Word;       // 0xC077
    Name:        string;     // "USB Optical Mouse" (또는 빈 문자열)
    DevicePath:  string;     // \\?\HID#VID_046D&PID_C077...
    Fingerprint: Int64;      // VID/PID 기반 unique 해시 (allow_list.json 용)
  end;

var
  // CustomPage + UI 요소
  HidScanPage:     TWizardPage;
  HidScanStatus:   TNewStaticText;
  HidDeviceList:   TNewCheckListBox;

  // 검색된 장치 목록 (HidScanPage 가 활성화될 때 채워짐).
  g_Devices: array of THidDevice;
  g_Scanned: Boolean;

function MakeHidGuid(): TGUID;
begin
  // {4D1E55B2-F16F-11CF-88CB-001111000030}
  Result.D1 := $4D1E55B2;
  Result.D2 := $F16F;
  Result.D3 := $11CF;
  Result.D4[0] := $88;
  Result.D4[1] := $CB;
  Result.D4[2] := $00;
  Result.D4[3] := $11;
  Result.D4[4] := $11;
  Result.D4[5] := $00;
  Result.D4[6] := $00;
  Result.D4[7] := $30;
end;

// ─── 헬퍼: hex 문자열 → 정수 ──────────────────────────────────────────────
function HexToInt(const s: string): Integer;
var
  i, c: Integer;
  ch: Char;
begin
  Result := 0;
  for i := 1 to Length(s) do
  begin
    ch := s[i];
    if (ch >= '0') and (ch <= '9') then c := Ord(ch) - Ord('0')
    else if (ch >= 'A') and (ch <= 'F') then c := Ord(ch) - Ord('A') + 10
    else if (ch >= 'a') and (ch <= 'f') then c := Ord(ch) - Ord('a') + 10
    else c := 0;
    Result := Result * 16 + c;
  end;
end;

// ─── DeviceID 에서 VID/PID 추출 ────────────────────────────────────────────
//   예) "HID\VID_046D&PID_C077\7&1A23FEBF&0&0000"
//   → vid=$046D, pid=$C077
procedure ExtractVidPid(const deviceId: string; var vid: Word; var pid: Word);
var
  posVid, posPid: Integer;
begin
  vid := 0;
  pid := 0;
  posVid := Pos('VID_', Uppercase(deviceId));
  posPid := Pos('PID_', Uppercase(deviceId));
  if (posVid > 0) and (Length(deviceId) >= posVid + 7) then
    vid := HexToInt(Copy(deviceId, posVid + 4, 4));
  if (posPid > 0) and (Length(deviceId) >= posPid + 7) then
    pid := HexToInt(Copy(deviceId, posPid + 4, 4));
end;

// ─── 같은 VID/PID 중복 제거 ────────────────────────────────────────────────
// HID 클래스는 한 USB 디바이스가 여러 HID interface(MI_00, MI_01, COL01..05)
// 로 쪼개져서 나옴. 사용자에겐 VID/PID 단위로만 보여주면 충분.
function AlreadyAdded(vid, pid: Word): Boolean;
var
  i: Integer;
begin
  Result := False;
  for i := 0 to GetArrayLength(g_Devices) - 1 do
    if (g_Devices[i].Vid = vid) and (g_Devices[i].Pid = pid) then
    begin
      Result := True;
      Exit;
    end;
end;

// ─── 한 줄 (Name=...|DeviceID=...) 파싱 ────────────────────────────────────
procedure ParseScanLine(const line: string);
var
  pipePos: Integer;
  namePart, idPart: string;
  vid, pid: Word;
  dev: THidDevice;
  n: Integer;
begin
  pipePos := Pos('|', line);
  if pipePos <= 0 then Exit;
  namePart := Trim(Copy(line, 1, pipePos - 1));
  idPart   := Trim(Copy(line, pipePos + 1, Length(line)));

  ExtractVidPid(idPart, vid, pid);
  if (vid = 0) and (pid = 0) then Exit;
  if AlreadyAdded(vid, pid) then Exit;

  dev.Vid := vid;
  dev.Pid := pid;
  dev.Name := namePart;
  dev.DevicePath := idPart;
  // fingerprint: VID/PID 만으로 unique 한 정수 (allow_list.json 호환용 임시값).
  // 솔루션의 진짜 fingerprint 알고리즘은 더 복잡하지만, 첫 실행 시 다시 계산되니까 OK.
  dev.Fingerprint := (Int64(vid) shl 16) or Int64(pid);

  n := GetArrayLength(g_Devices);
  SetArrayLength(g_Devices, n + 1);
  g_Devices[n] := dev;
end;

// ─── PowerShell 로 HID 장치 enum ────────────────────────────────────────────
//   - Get-PnpDevice 로 현재 연결된 HIDClass 장치 목록 가져옴.
//   - Get-PnpDeviceProperty -KeyName 'DEVPKEY_Device_BusReportedDeviceDesc' 로
//     USB 가 보고하는 진짜 device description (제조사 펌웨어 product string) 추출.
//     이게 없는 디바이스는 FriendlyName 으로 fallback.
//   - 출력 인코딩: UTF-16 LE BOM (Out-File -Encoding Unicode)
//     → Inno Setup LoadStringsFromFile 가 자동 인식. 한국어/유니코드 정상.
function EnumerateHidDevices(): Boolean;
var
  scanScript, scanOutput: string;
  resultCode: Integer;
  lines: TArrayOfString;
  i: Integer;
begin
  SetArrayLength(g_Devices, 0);

  scanScript := ExpandConstant('{tmp}\hid_scan.ps1');
  scanOutput := ExpandConstant('{tmp}\hid_scan.txt');

  SaveStringToFile(scanScript,
    '$ErrorActionPreference = ''SilentlyContinue''' + #13#10 +
    'Get-PnpDevice -PresentOnly -Class HIDClass |' + #13#10 +
    '  Where-Object { $_.Status -eq ''OK'' } |' + #13#10 +
    '  ForEach-Object {' + #13#10 +
    '    $busDesc = $null' + #13#10 +
    '    try {' + #13#10 +
    '      $prop = Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName ''DEVPKEY_Device_BusReportedDeviceDesc'' -ErrorAction SilentlyContinue' + #13#10 +
    '      if ($prop -and $prop.Data) { $busDesc = $prop.Data }' + #13#10 +
    '    } catch {}' + #13#10 +
    '    $displayName = if ($busDesc) { $busDesc } else { $_.FriendlyName }' + #13#10 +
    '    if (-not $displayName) { $displayName = ''(Unknown HID device)'' }' + #13#10 +
    '    "$displayName|$($_.InstanceId)"' + #13#10 +
    '  } |' + #13#10 +
    '  Out-File -FilePath ''' + scanOutput + ''' -Encoding UTF8' + #13#10,
    False);

  // 64-bit PowerShell 호출 (SysNative redirect — 32-bit 인스톨러용)
  Exec(ExpandConstant('{sysnative}\WindowsPowerShell\v1.0\powershell.exe'),
       '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + scanScript + '"',
       '', SW_HIDE, ewWaitUntilTerminated, resultCode);

  if not FileExists(scanOutput) then
  begin
    // fallback: 일반 powershell.exe (WOW64 redirect)
    Exec('powershell.exe',
         '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + scanScript + '"',
         '', SW_HIDE, ewWaitUntilTerminated, resultCode);
  end;

  if not FileExists(scanOutput) then
  begin
    Result := False;
    Exit;
  end;

  if not LoadStringsFromFile(scanOutput, lines) then
  begin
    Result := False;
    Exit;
  end;

  for i := 0 to GetArrayLength(lines) - 1 do
    ParseScanLine(lines[i]);

  // 임시 파일 정리.
  DeleteFile(scanScript);
  DeleteFile(scanOutput);

  Result := True;
end;

procedure InitializeWizard();
begin
  HidScanPage := CreateCustomPage(
    wpSelectTasks,
    ExpandConstant('{cm:HidScanCaption}'),
    ExpandConstant('{cm:HidScanDescription}')
  );

  // 임시 상태 라벨 (스캔 중 / 결과 없음 표시).
  HidScanStatus := TNewStaticText.Create(HidScanPage);
  HidScanStatus.Parent := HidScanPage.Surface;
  HidScanStatus.Left   := 0;
  HidScanStatus.Top    := 0;
  HidScanStatus.Width  := HidScanPage.SurfaceWidth;
  HidScanStatus.Caption := ExpandConstant('{cm:HidScanInProgress}');

  // 디바이스 체크 리스트 (스캔 후 채움).
  HidDeviceList := TNewCheckListBox.Create(HidScanPage);
  HidDeviceList.Parent  := HidScanPage.Surface;
  HidDeviceList.Left    := 0;
  HidDeviceList.Top     := ScaleY(24);
  HidDeviceList.Width   := HidScanPage.SurfaceWidth;
  HidDeviceList.Height  := HidScanPage.SurfaceHeight - ScaleY(24);
  HidDeviceList.Visible := False;  // 스캔 끝나면 보이게.

  g_Scanned := False;
end;

// 페이지가 활성화될 때 스캔 (한 번만).
procedure CurPageChanged(CurPageID: Integer);
var
  i: Integer;
  caption: string;
begin
  if (CurPageID = HidScanPage.ID) and (not g_Scanned) then
  begin
    g_Scanned := True;
    EnumerateHidDevices();

    if GetArrayLength(g_Devices) = 0 then
    begin
      HidScanStatus.Caption := ExpandConstant('{cm:HidScanNoDevices}');
    end
    else
    begin
      HidScanStatus.Visible  := False;
      HidDeviceList.Visible  := True;
      for i := 0 to GetArrayLength(g_Devices) - 1 do
      begin
        // 일반 사용자에겐 VID/PID hex 의미 없음 → 이름만 표시.
        caption := g_Devices[i].Name;
        HidDeviceList.AddCheckBox(caption, '', 0, True, True, False, True, nil);
      end;
    end;
  end;
end;

// ─── allow_list.json 직접 생성 ─────────────────────────────────────────────
// 솔루션의 Policy::Save() 와 동일한 JSON 포맷:
//   {
//     "whitelist": [
//       { "vid": "046D", "pid": "C077", "name": "USB Optical Mouse", "fingerprint": "0x0000046D0000C077" },
//       ...
//     ],
//     "blacklist": [...]
//   }
// 솔루션 첫 실행 시 이 파일을 그대로 로드 → 초기 스캔 skip.
procedure WriteAllowListJson(const path: string);
var
  i: Integer;
  json: string;
  isFirstWhite, isFirstBlack: Boolean;
  allowed: Boolean;
begin
  json := '{' + #13#10;

  // ── whitelist ──
  json := json + '    "whitelist": [';
  isFirstWhite := True;
  for i := 0 to GetArrayLength(g_Devices) - 1 do
  begin
    allowed := HidDeviceList.Checked[i];
    if allowed then
    begin
      if not isFirstWhite then json := json + ',';
      json := json + #13#10 + '        {' + #13#10;
      json := json + '            "vid": "'         + Format('%4.4X', [g_Devices[i].Vid]) + '",' + #13#10;
      json := json + '            "pid": "'         + Format('%4.4X', [g_Devices[i].Pid]) + '",' + #13#10;
      json := json + '            "name": "'        + g_Devices[i].Name + '",' + #13#10;
      json := json + '            "fingerprint": "0x' + Format('%16.16X', [g_Devices[i].Fingerprint]) + '"' + #13#10;
      json := json + '        }';
      isFirstWhite := False;
    end;
  end;
  if not isFirstWhite then json := json + #13#10 + '    ';
  json := json + '],' + #13#10;

  // ── blacklist ──
  json := json + '    "blacklist": [';
  isFirstBlack := True;
  for i := 0 to GetArrayLength(g_Devices) - 1 do
  begin
    allowed := HidDeviceList.Checked[i];
    if not allowed then
    begin
      if not isFirstBlack then json := json + ',';
      json := json + #13#10 + '        {' + #13#10;
      json := json + '            "vid": "'         + Format('%4.4X', [g_Devices[i].Vid]) + '",' + #13#10;
      json := json + '            "pid": "'         + Format('%4.4X', [g_Devices[i].Pid]) + '",' + #13#10;
      json := json + '            "name": "'        + g_Devices[i].Name + '",' + #13#10;
      json := json + '            "fingerprint": "0x' + Format('%16.16X', [g_Devices[i].Fingerprint]) + '"' + #13#10;
      json := json + '        }';
      isFirstBlack := False;
    end;
  end;
  if not isFirstBlack then json := json + #13#10 + '    ';
  json := json + ']' + #13#10;

  json := json + '}' + #13#10;

  // 솔루션 Policy::Load() 는 UTF-8 로 파일을 읽음.
  // SaveStringToFile 은 ANSI(시스템 codepage) 로 저장 → 비-ASCII 는 깨질 위험.
  // 다만 디바이스 이름은 거의 영어 (USB 표준), JSON 의 다른 부분은 ASCII 만.
  // 한국어 generic name 이 들어가도 솔루션 첫 실행 시 다시 정확히 갱신됨.
  SaveStringToFile(path, json, False);
end;

// 마법사 진행 — HidScanPage 에서 Next 누를 때 결정 저장.
function NextButtonClick(CurPageID: Integer): Boolean;
var
  appDataDir, listPath: string;
begin
  if CurPageID = HidScanPage.ID then
  begin
    // 디바이스 한 개도 없으면 그냥 skip.
    if GetArrayLength(g_Devices) > 0 then
    begin
      appDataDir := ExpandConstant('{userappdata}\AbangAll');
      ForceDirectories(appDataDir);
      listPath := appDataDir + '\allow_list.json';
      WriteAllowListJson(listPath);
    end;
  end;
  Result := True;  // 항상 다음으로 진행
end;

// ============================================================================
// 언인스톨 처리 (기존 코드)
// ============================================================================
// ----------------------------------------------------------------------------
// 언인스톨 시 처리:
//   1) 실행 중인 AbangAll.exe 프로세스가 있으면 강제 종료
//      (트레이 상주 앱이라 사용자가 미처 못 끄는 케이스 처리)
//   2) %APPDATA%\AbangAll\ 폴더 삭제 여부 사용자에게 묻기
//      - YES -> 폴더 통째로 삭제
//      - NO  -> 보존 (재설치 시 정책 유지)
//
// 주의: 언인스톨러는 admin 권한으로 실행됨 (PrivilegesRequired=admin).
//       그래서 taskkill 호출 가능.
//       ExpandConstant 의 userappdata 가 인스톨한 사용자의 AppData 를 가리킴.
// ----------------------------------------------------------------------------

// 언인스톨 시작 직전 — 실행 중인 프로세스 강제 종료.
function InitializeUninstall(): Boolean;
var
  resultCode: Integer;
begin
  // taskkill /F /IM AbangAll.exe — 실행 중이 아니면 그냥 비-0 반환하고 끝.
  Exec(ExpandConstant('{sys}\taskkill.exe'),
       '/F /IM AbangAll.exe',
       '',
       SW_HIDE,
       ewWaitUntilTerminated,
       resultCode);
  // 프로세스 종료 후 파일 lock 해제 대기 (짧게).
  Sleep(500);
  Result := True;
end;

// 언인스톨 완료 후 — AppData 정책 폴더 삭제 여부 묻기.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  appDataDir: string;
  resp: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    appDataDir := ExpandConstant('{userappdata}\AbangAll');
    if DirExists(appDataDir) then
    begin
      resp := MsgBox(
        ExpandConstant('{cm:UninstallAppDataPrompt}') + #13#10 + #13#10 +
        ExpandConstant('{cm:UninstallAppDataKeep}'),
        mbConfirmation, MB_YESNO or MB_DEFBUTTON2
      );
      if resp = IDYES then
      begin
        DelTree(appDataDir, True, True, True);
      end;
    end;
  end;
end;
