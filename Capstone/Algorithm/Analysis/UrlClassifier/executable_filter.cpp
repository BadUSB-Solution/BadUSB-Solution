#include "executable_filter.h"

namespace UrlClassifier::ExecutableFilter
{
    static const wchar_t* DANGEROUS_EXTS[] = {
        L".exe", L".dll", L".scr",          // 네이티브 실행
        L".ps1", L".psm1",                  // PowerShell
        L".bat", L".cmd",                   // CMD
        L".hta", L".vbs", L".js", L".jse",  // 스크립트
        L".lnk",                            // 바로가기 (페이로드 트리거)
        L".msi",                            // 인스톨러
    };

    uint32_t Check(const ParsedUrl& p)
    {
        if (p.path.empty()) return 0;

        // 쿼리스트링/프래그먼트 제거된 path 부분만 검사
        std::wstring path = p.path;
        size_t q = path.find_first_of(L"?#");
        if (q != std::wstring::npos) path = path.substr(0, q);

        std::wstring lower = ToLower(path);
        for (auto* ext : DANGEROUS_EXTS) {
            if (EndsWith(lower, ext))
                return URL_FLAG_EXE_EXTENSION;
        }
        return 0;
    }
}
