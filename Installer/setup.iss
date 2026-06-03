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
#define AppVersion     "1.0.4"
#define AppPublisher   "AbangAll Team"
#define AppExeName     "AbangAll.exe"
; Qt UI 빌드 산출물 디렉터리 (CMake가 windeployqt까지 자동 처리해서 모든 Qt 의존성 동봉됨)
#ifndef SourceDir
#define SourceDir      "..\build-qt\Release"
#endif
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
korean.HidScanInProgress=현재 연결된 HID 장치를 검색하는 중입니다. (수 초에서 수 분이 소요될 수 있습니다.)
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
Source: "assets\icon.ico"; DestDir: "{app}"; DestName: "app_icon.ico"; Flags: ignoreversion
Source: "assets\fonts\*"; DestDir: "{app}\fonts"; Flags: ignoreversion recursesubdirs createallsubdirs
; HID 스캔 PowerShell 스크립트 — 인스톨러 EXE 안에 묶고, {tmp} 에 일시 추출
;   본체 hid_fingerprint.cpp 와 동일 로직 (ContainerId 통합 + 부모 chain 이름 탐색)
Source: "scripts\hid_scan.ps1"; Flags: dontcopy

; ── Qt6 핵심 DLL (windeployqt가 build-qt/Release에 자동 배치)
Source: "{#SourceDir}\Qt6Core.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Gui.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Widgets.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Network.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Qt6Svg.dll";        DestDir: "{app}"; Flags: ignoreversion

; ── 시스템 의존 DLL
Source: "{#SourceDir}\D3Dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\opengl32sw.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\icuuc.dll";          DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

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
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\app_icon.ico"; Tasks: menuicon
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"; IconFilename: "{app}\app_icon.ico"; Tasks: menuicon

; 바탕화면 단축키
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\app_icon.ico"; Tasks: desktopicon

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
//
// 실제 HID 열거 / 이름 추출 / fingerprint 계산은 Installer\scripts\hid_scan.ps1
// 가 담당. 이 스크립트는 본체 Capstone 의 hid_fingerprint.cpp / hid_scan.cpp
// 로직을 그대로 포팅한 것 → 솔루션과 결과가 일관됨.
// Pascal 측은 PS 출력 (한 줄 = Name|InstanceId|FpHex16|VidHex4|PidHex4) 을
// 파싱해서 체크 리스트로만 띄움.
// ============================================================================

// ─── 데이터 타입 ──────────────────────────────────────────────────────────
type
  THidDevice = record
    Vid:         Word;     // 0x046D — 없으면 0 (Bluetooth / ACPI 등)
    Pid:         Word;     // 0xC077 — 없으면 0
    Name:        string;   // 본체 SelectBestCandidateName 결과
    DevicePath:  string;   // Pnp InstanceId (예: HID\VID_046D&PID_C077\7&...)
    Fingerprint: string;   // FNV-1a 64 결과 16 char hex 대문자 (0x prefix 없음)
  end;

var
  // CustomPage + UI 요소
  HidScanPage:     TWizardPage;
  HidScanStatus:   TNewStaticText;
  HidScanProgress: TNewProgressBar;   // 스캔 중 marquee(불확정) 로딩 애니메이션
  HidDeviceList:   TNewCheckListBox;

  // 검색된 장치 목록 (HidScanPage 가 활성화될 때 채워짐).
  g_Devices: array of THidDevice;
  g_Scanned: Boolean;

// ─── hex 문자열 → 정수 ────────────────────────────────────────────────────
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

// ─── fingerprint 기준 중복 제거 ───────────────────────────────────────────
// PS 측에서 이미 dedupe 하지만, 호출 실패/병합 등에 대비해 Pascal 측에서도 한 번 더.
function AlreadyAdded(const fingerprint: string): Boolean;
var
  i: Integer;
begin
  Result := False;
  for i := 0 to GetArrayLength(g_Devices) - 1 do
    if g_Devices[i].Fingerprint = fingerprint then
    begin
      Result := True;
      Exit;
    end;
end;

// ─── 한 줄 ("Name|InstanceId|FpHex16|VidHex4|PidHex4") 파싱 ──────────────
//   Pascal Script 에는 SplitString 이 없어 직접 분리.
procedure ParseScanLine(const line: string);
var
  parts: array of string;
  i, partIdx, startPos: Integer;
  dev: THidDevice;
  n: Integer;
begin
  if line = '' then Exit;

  SetArrayLength(parts, 0);
  partIdx := 0;
  startPos := 1;
  for i := 1 to Length(line) do
  begin
    if line[i] = '|' then
    begin
      SetArrayLength(parts, partIdx + 1);
      parts[partIdx] := Copy(line, startPos, i - startPos);
      partIdx := partIdx + 1;
      startPos := i + 1;
    end;
  end;
  // 마지막 토큰
  SetArrayLength(parts, partIdx + 1);
  parts[partIdx] := Copy(line, startPos, Length(line) - startPos + 1);

  if GetArrayLength(parts) < 5 then Exit;

  dev.Name        := Trim(parts[0]);
  dev.DevicePath  := Trim(parts[1]);
  dev.Fingerprint := Trim(parts[2]);
  dev.Vid         := HexToInt(Trim(parts[3]));
  dev.Pid         := HexToInt(Trim(parts[4]));

  if dev.Fingerprint = '' then Exit;
  if AlreadyAdded(dev.Fingerprint) then Exit;

  if dev.Name = '' then
  begin
    if (dev.Vid <> 0) or (dev.Pid <> 0) then
      dev.Name := Format('HID Device (VID_%4.4X&PID_%4.4X)', [dev.Vid, dev.Pid])
    else
      dev.Name := 'Unknown HID Device';
  end;

  n := GetArrayLength(g_Devices);
  SetArrayLength(g_Devices, n + 1);
  g_Devices[n] := dev;
end;

// ─── PowerShell 로 HID 장치 enum ──────────────────────────────────────────
//   - hid_scan.ps1 을 인스톨러 EXE 에서 {tmp} 로 추출 (ExtractTemporaryFile)
//   - 64-bit PowerShell (SysNative) 로 실행 — 32-bit 인스톨러 호환
//   - 실패 시 기본 powershell.exe (WOW64) 로 fallback
function EnumerateHidDevices(): Boolean;
var
  scanScript, scanOutput: string;
  resultCode: Integer;
  lines: TArrayOfString;
  i: Integer;
begin
  SetArrayLength(g_Devices, 0);

  // hid_scan.ps1 을 {tmp} 에 추출
  ExtractTemporaryFile('hid_scan.ps1');
  scanScript := ExpandConstant('{tmp}\hid_scan.ps1');
  scanOutput := ExpandConstant('{tmp}\hid_scan.txt');

  Exec(ExpandConstant('{sysnative}\WindowsPowerShell\v1.0\powershell.exe'),
       '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + scanScript +
       '" -OutputPath "' + scanOutput + '"',
       '', SW_HIDE, ewWaitUntilTerminated, resultCode);

  if not FileExists(scanOutput) then
  begin
    Exec('powershell.exe',
         '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + scanScript +
         '" -OutputPath "' + scanOutput + '"',
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

  // 스캔 중 로딩 애니메이션 — marquee(불확정) 진행바.
  //   Exec(... ewWaitUntilTerminated ...) 가 대기 중에도 메시지 루프를 펌프하므로
  //   PowerShell 스캔이 끝날 때까지 이 진행바가 자동으로 좌우로 흐른다.
  HidScanProgress := TNewProgressBar.Create(HidScanPage);
  HidScanProgress.Parent  := HidScanPage.Surface;
  HidScanProgress.Left    := 0;
  HidScanProgress.Top     := ScaleY(24);
  HidScanProgress.Width   := HidScanPage.SurfaceWidth;
  HidScanProgress.Height  := ScaleY(16);
  HidScanProgress.Style   := npbstMarquee;
  HidScanProgress.Visible := False;  // 스캔 시작 시 보이게.

  // 디바이스 체크 리스트 (스캔 후 채움).
  HidDeviceList := TNewCheckListBox.Create(HidScanPage);
  HidDeviceList.Parent  := HidScanPage.Surface;
  HidDeviceList.Left    := 0;
  HidDeviceList.Top     := ScaleY(48);
  HidDeviceList.Width   := HidScanPage.SurfaceWidth;
  HidDeviceList.Height  := HidScanPage.SurfaceHeight - ScaleY(48);
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

    // 스캔 동안에는 [뒤로]/[다음]만 비활성화하고 [취소]는 그대로 눌러서
    // 설치를 중단할 수 있게 둔다. (스캔이 끝나면 다시 활성화)
    WizardForm.BackButton.Enabled   := False;
    WizardForm.NextButton.Enabled   := False;
    WizardForm.CancelButton.Enabled := True;

    // 로딩 시작 — "검색 중" 라벨 + marquee 진행바 표시.
    HidScanStatus.Caption  := ExpandConstant('{cm:HidScanInProgress}');
    HidScanStatus.Visible  := True;
    HidScanProgress.Visible := True;
    HidScanProgress.Show;
    WizardForm.Refresh;  // 스캔 차단 전에 라벨/진행바/버튼 상태를 즉시 그려둔다.

    EnumerateHidDevices();

    // 스캔 종료 — 진행바 숨김 + 이동 버튼 복구.
    HidScanProgress.Visible := False;
    WizardForm.BackButton.Enabled := True;
    WizardForm.NextButton.Enabled := True;

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
      json := json + '            "fingerprint": "0x' + g_Devices[i].Fingerprint + '"' + #13#10;
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
      json := json + '            "fingerprint": "0x' + g_Devices[i].Fingerprint + '"' + #13#10;
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
