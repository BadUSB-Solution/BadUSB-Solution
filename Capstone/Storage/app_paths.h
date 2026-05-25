#pragma once
#include <string>

// %APPDATA%\AbangAll\ 아래에 두는 사용자 데이터 파일 경로 헬퍼.
// - 폴더는 최초 호출 시 1회만 생성하고 캐싱한다.
// - 파일 hidden 속성 관련 Windows 동작도 여기서 흡수한다.
namespace Storage
{
    // %APPDATA%\AbangAll\ 디렉터리 경로. 없으면 만들고 캐싱.
    const std::wstring& GetAbangAllDir();

    // 도메인별 파일 경로.
    std::wstring GetAllowListPath();   // Policy 화이트/블랙리스트
    std::wstring GetSettingsPath();    // 앱 설정

    // Windows 한정: hidden 속성이 걸린 파일은 std::ofstream(trunc)으로
    // 열 때 실패할 수 있다. 쓰기 전에 UnhideForWrite, 닫은 뒤 MarkHidden.
    void UnhideForWrite(const std::wstring& path);
    void MarkHidden(const std::wstring& path);
}
