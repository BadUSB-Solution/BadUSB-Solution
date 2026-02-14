Windows에서 비정상 키보드 입력 패턴을 감지하고 차단한다.

## 1. 요구사항

- Windows 10/11 (x64)
- Visual Studio 2019 Build Tools 이상 (MSVC)
- CMake 3.16 이상

## 2. 프로젝트 구성

- `src/`
- `include/`
- `imgui/`
- `CMakeLists.txt`
- `config.json`

## 3. 빌드 (CMake)

프로젝트 루트(`BadUSBGuardian`)에서 실행:

```powershell
cmake -S . -B build -G "NMake Makefiles" -DBADUSB_WITH_IMGUI=ON -DIMGUI_DIR=imgui
cmake --build build
```

## 4. 실행

```powershell
cd build
.\BadUSBGuardian.exe
```

실행 후 `config.json` 기준으로 임계값이 적용되며, 로그는 `badusb_guardian.log`에 기록된다.

## 5. 테스트

```powershell
cmake --build build --target BadUSBGuardianTest
```

또는 기존 스크립트 사용:

```powershell
.\build_and_test.bat
```

## 6. 설정 파일

`config.json`에서 아래 값을 조정할 수 있다.

- `warn_cps`
- `block_cps`
- `warn_score`
- `block_score`
- `keywords`
