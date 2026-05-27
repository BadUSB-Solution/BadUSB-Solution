# AbangAll Installer

Inno Setup 기반 인스톨러.

## 사전 요구사항

- **Inno Setup 6+** 설치 (https://jrsoftware.org/isdl.php)
- **Visual Studio 2022 Build Tools** (MSBuild + v143 toolset)
- **Capstone (Release|x64)** 빌드 산출물:
  - `..\Capstone\x64\Release\Capstone.exe`
  - `..\Capstone\x64\Release\matchingDB.db`

## 빌드 방법

```powershell
.\scripts\build_setup.ps1
```

→ `Capstone` 자동 빌드 + `setup.iss` 컴파일 → `output\AbangAll-Installer.exe` 생성.

수동으로 하려면:
```powershell
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" setup.iss
```

## 인스톨러 동작

| 단계 | 동작 |
|---|---|
| 1. 환영 | 마법사 시작 |
| 2. 설치 경로 | 기본 `C:\Program Files\AbangAll\` |
| 3. 추가 아이콘 | 시작 메뉴 (기본 ON) / 바탕화면 (기본 OFF) 선택 |
| 4. 설치 | `AbangAll.exe`, `matchingDB.db` 배포 |
| 5. 완료 | "AbangAll 즉시 실행" 체크박스 (기본 ON) |

설치 후 첫 실행 시 솔루션이 자동으로:
1. 기존 HID 장치 스캔
2. 일괄 승인 다이얼로그 표시
3. 사용자 결정 → `%APPDATA%\AbangAll\allow_list.json` 저장

## 파일 위치 정리

```
C:\Program Files\AbangAll\          ← 인스톨러가 배치
├── AbangAll.exe                    (Capstone.exe 리네임됨)
├── matchingDB.db
└── unins000.exe / unins000.dat     (Inno Setup 자동 생성)

%APPDATA%\AbangAll\                  ← 솔루션 첫 실행 시 자동 생성
├── settings.json
├── allow_list.json
└── log.json
```

## 언인스톨

`설정 → 앱` 에서 `AbangAll` 제거.

언인스톨 시 `%APPDATA%\AbangAll\` 폴더 삭제 여부를 사용자에게 묻는다 (`setup.iss` `[Code]` 섹션의 `CurUninstallStepChanged`).

- YES → 정책/설정/로그 통째 삭제
- NO  → 보존 (재설치 시 정책 유지)
