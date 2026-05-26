// CapstoneTest — 알고리즘 파이프라인 단독 테스트
// 사용법:
//   CapstoneTest.exe                          # 하드코딩 샘플 실행
//   CapstoneTest.exe --seq "Win R N O T E P A D Enter"
//   CapstoneTest.exe path/to/input.txt        # 파일 (UTF-8)
//   CapstoneTest.exe --interval 5             # 키 간격 ms (기본 50)

#include "test_key_parser.h"
#include "test_runner.h"
#include "threat_matcher.h"

#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::wstring ReadFile(const std::wstring& path)
{
    // _wfopen 으로 wide path 직접 사용 (narrow 변환 회피)
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp) return {};

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { fclose(fp); return {}; }

    std::string utf8(size, '\0');
    size_t got = fread(utf8.data(), 1, (size_t)size, fp);
    fclose(fp);
    if (got == 0) return {};

    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)got,
                                 nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)got,
                        out.data(), n);
    return out;
}

static const wchar_t* HARDCODED_SAMPLE =
    L"Win R P O W E R S H E L L Space - E P Space B Y P A S S Enter";

int wmain(int argc, wchar_t* argv[])
{
    // 콘솔 UTF-16 출력 설정 (한글 정상 표시)
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    // 인자 파싱
    std::wstring inputSeq;
    DWORD intervalMs = 50;

    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--seq" && i + 1 < argc) {
            inputSeq = argv[++i];
        } else if (a == L"--interval" && i + 1 < argc) {
            intervalMs = (DWORD)_wtoi(argv[++i]);
        } else if (a.size() > 2 && a.substr(0, 2) == L"--") {
            std::wcerr << L"Unknown option: " << a << L"\n";
            return 1;
        } else {
            // 파일 경로로 간주
            inputSeq = ReadFile(a);
            if (inputSeq.empty()) {
                std::wcerr << L"Cannot read file: " << a << L"\n";
                return 2;
            }
        }
    }

    if (inputSeq.empty()) {
        std::wcout << L"(no input given, running hardcoded sample)\r\n\r\n";
        inputSeq = HARDCODED_SAMPLE;
    }

    std::wcout << L"Input sequence: " << inputSeq << L"\r\n";
    std::wcout << L"Interval: " << intervalMs << L"ms\r\n\r\n";

    // ThreatMatcher DB 경로 (실행 디렉토리)
    ThreatMatcher::SetDatabasePath(L"matchingDB.db");

    // 파싱 → 파이프라인 실행 → 출력
    TestKeyParser::Options opts;
    opts.intervalMs = intervalMs;
    auto keys = TestKeyParser::ParseSequence(inputSeq, opts);
    if (keys.empty()) {
        std::wcerr << L"No keys parsed from input.\n";
        return 3;
    }

    auto result = TestRunner::Run(keys);
    std::wcout << TestRunner::FormatResult(keys, result);

    ThreatMatcher::Shutdown();
    return 0;
}
