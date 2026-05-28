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
#define SourceExePath  "..\Capstone\x64\Release\Capstone.exe"
#define SourceDbPath   "..\Capstone\x64\Release\matchingDB.db"

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
korean.UninstallAppDataPrompt=정책 / 설정 / 로그 폴더도 삭제하시겠습니까?
korean.UninstallAppDataKeep=아니오를 선택하면 다음 설치 시 기존 정책이 그대로 유지됩니다.

; ─── English ───────────────────────────────────────────────────────────
english.AdditionalIconsGroup=Additional icons:
english.MenuIconDesc=Create a Start Menu shortcut
english.DesktopIconDesc=Create a Desktop shortcut
english.UninstallAppDataPrompt=Do you also want to remove the policy / settings / log folder?
english.UninstallAppDataKeep=Selecting No will preserve existing policy for next install.

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
; 본체 — Capstone.exe 를 AbangAll.exe 로 리네임해서 배포
Source: "{#SourceExePath}"; DestDir: "{app}"; DestName: "{#AppExeName}"; Flags: ignoreversion
; 시그니처 DB
Source: "{#SourceDbPath}"; DestDir: "{app}"; Flags: ignoreversion

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
        appDataDir + #13#10 + #13#10 +
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
